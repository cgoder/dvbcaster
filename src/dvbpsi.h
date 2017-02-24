/*****************************************************************************
 * vlc_psi.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 VLC authors and VideoLAN
 * Copyright © 2007-2008 Rémi Denis-Courmont
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef DVB_PSI_H_
#define DVBPSI_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdint.h>

struct dvbpsi_psi_section_s
{
  /* non-specific section data */
  uint8_t       i_table_id;             /*!< table_id */
  bool          b_syntax_indicator;     /*!< section_syntax_indicator */
  bool          b_private_indicator;    /*!< private_indicator */
  uint16_t      i_length;               /*!< section_length */

  /* used if b_syntax_indicator is true */
  uint16_t      i_extension;            /*!< table_id_extension */
                                        /*!< transport_stream_id for a
                                             PAT section */
  uint8_t       i_version;              /*!< version_number */
  bool          b_current_next;         /*!< current_next_indicator */
  uint8_t       i_number;               /*!< section_number */
  uint8_t       i_last_number;          /*!< last_section_number */

  /* non-specific section data */
  /* the content is table-specific */
  uint8_t *     p_data;                 /*!< complete section */
  uint8_t *     p_payload_start;        /*!< payload start */
  uint8_t *     p_payload_end;          /*!< payload end */

  /* used if b_syntax_indicator is true */
  uint32_t      i_crc;                  /*!< CRC_32 */

  /* list handling */
  struct dvbpsi_psi_section_s *         p_next;         /*!< next element of
                                                             the list */
};

typedef struct dvbpsi_psi_section_s dvbpsi_psi_section_t;

/*****************************************************************************
 * dvbpsi_pat_program_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_pat_program_s
 * \brief PAT program structure.
 *
 * This structure is used to store a decoded PAT program.
 * (ISO/IEC 13818-1 section 2.4.4.3).
 */
/*!
 * \typedef struct dvbpsi_pat_program_s dvbpsi_pat_program_t
 * \brief dvbpsi_pat_program_t type definition.
 */
typedef struct dvbpsi_pat_program_s
{
  uint16_t                      i_number;               /*!< program_number */
  uint16_t                      i_pid;                  /*!< PID of NIT/PMT */

  struct dvbpsi_pat_program_s * p_next;                 /*!< next element of
                                                             the list */

} dvbpsi_pat_program_t;


/*****************************************************************************
 * dvbpsi_pat_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_pat_s
 * \brief PAT structure.
 *
 * This structure is used to store a decoded PAT.
 * (ISO/IEC 13818-1 section 2.4.4.3).
 */
/*!
 * \typedef struct dvbpsi_pat_s dvbpsi_pat_t
 * \brief dvbpsi_pat_t type definition.
 */
typedef struct dvbpsi_pat_s
{
  uint16_t                  i_ts_id;            /*!< transport_stream_id */
  uint8_t                   i_version;          /*!< version_number */
  bool                      b_current_next;     /*!< current_next_indicator */

  dvbpsi_pat_program_t *    p_first_program;    /*!< program list */

} dvbpsi_pat_t;

/*!
 * \fn void dvbpsi_pat_init(dvbpsi_pat_t* p_pat, uint16_t i_ts_id,
                           uint8_t i_version, bool b_current_next)
 * \brief Initialize a user-allocated dvbpsi_pat_t structure.
 * \param p_pat pointer to the PAT structure
 * \param i_ts_id transport stream ID
 * \param i_version PAT version
 * \param b_current_next current next indicator
 * \return nothing.
 */
void dvbpsi_pat_init(dvbpsi_pat_t* p_pat, uint16_t i_ts_id, uint8_t i_version,
                     bool b_current_next);

/*****************************************************************************
 * dvbpsi_pat_empty/dvbpsi_pat_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_pat_empty(dvbpsi_pat_t* p_pat)
 * \brief Clean a dvbpsi_pat_t structure.
 * \param p_pat pointer to the PAT structure
 * \return nothing.
 */
void dvbpsi_pat_empty(dvbpsi_pat_t* p_pat);

/*****************************************************************************
 * dvbpsi_pat_program_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_pat_program_t* dvbpsi_pat_program_add(dvbpsi_pat_t* p_pat,
                                                    uint16_t i_number,
                                                    uint16_t i_pid)
 * \brief Add a program at the end of the PAT.
 * \param p_pat pointer to the PAT structure
 * \param i_number program number
 * \param i_pid PID of the NIT/PMT
 * \return a pointer to the added program.
 */
dvbpsi_pat_program_t* dvbpsi_pat_program_add(dvbpsi_pat_t* p_pat,
                                             uint16_t i_number, uint16_t i_pid);

/*****************************************************************************
 * dvbpsi_pat_sections_generate
 *****************************************************************************/
/*!
 * \fn dvbpsi_psi_section_t* dvbpsi_pat_sections_generate(dvbpsi_t *p_dvbpsi, dvbpsi_pat_t* p_pat,
                                                   int i_max_pps);
 * \brief PAT generator.
 * \param p_dvbpsi handle to dvbpsi with attached decoder
 * \param p_pat pointer to the PAT structure
 * \param i_max_pps limitation of the number of program in each section
 * (max: 253).
 * \return a pointer to the list of generated PSI sections.
 *
 * Generate PAT sections based on the dvbpsi_pat_t structure.
 */
dvbpsi_psi_section_t* dvbpsi_pat_sections_generate(dvbpsi_pat_t* p_pat, int i_max_pps);


typedef struct dvbpsi_descriptor_s
{
  uint8_t                       i_tag;          /*!< descriptor_tag */
  uint8_t                       i_length;       /*!< descriptor_length */

  uint8_t *                     p_data;         /*!< content */

  struct dvbpsi_descriptor_s *  p_next;         /*!< next element of
                                                     the list */

  void *                        p_decoded;      /*!< decoded descriptor */

} dvbpsi_descriptor_t;

dvbpsi_psi_section_t *dvbpsi_NewPSISection(int i_max_size);

/*****************************************************************************
 * dvbpsi_DeletePSISections
 *****************************************************************************
 * Destruction of a dvbpsi_psi_section_t structure.
 *****************************************************************************/
void dvbpsi_DeletePSISections(dvbpsi_psi_section_t *p_section);


/*****************************************************************************
 * dvbpsi_pmt_es_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_pmt_es_s
 * \brief PMT ES structure.
 *
 * This structure is used to store a decoded ES description.
 * (ISO/IEC 13818-1 section 2.4.4.8).
 */
/*!
 * \typedef struct dvbpsi_pmt_es_s dvbpsi_pmt_es_t
 * \brief dvbpsi_pmt_es_t type definition.
 */
typedef struct dvbpsi_pmt_es_s
{
  uint8_t                       i_type;                 /*!< stream_type */
  uint16_t                      i_pid;                  /*!< elementary_PID */

  dvbpsi_descriptor_t *         p_first_descriptor;     /*!< descriptor list */

  struct dvbpsi_pmt_es_s *      p_next;                 /*!< next element of
                                                             the list */

} dvbpsi_pmt_es_t;

/*****************************************************************************
 * dvbpsi_pmt_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_pmt_s
 * \brief PMT structure.
 *
 * This structure is used to store a decoded PMT.
 * (ISO/IEC 13818-1 section 2.4.4.8).
 */
/*!
 * \typedef struct dvbpsi_pmt_s dvbpsi_pmt_t
 * \brief dvbpsi_pmt_t type definition.
 */
typedef struct dvbpsi_pmt_s
{
  uint16_t                  i_program_number;   /*!< program_number */
  uint8_t                   i_version;          /*!< version_number */
  bool                      b_current_next;     /*!< current_next_indicator */

  uint16_t                  i_pcr_pid;          /*!< PCR_PID */

  dvbpsi_descriptor_t *     p_first_descriptor; /*!< descriptor list */

  dvbpsi_pmt_es_t *         p_first_es;         /*!< ES list */

} dvbpsi_pmt_t;

/*****************************************************************************
 * dvbpsi_pmt_init/dvbpsi_pmt_new
 *****************************************************************************/
/*!
 * \fn void dvbpsi_pmt_init(dvbpsi_pmt_t* p_pmt, uint16_t i_program_number,
                           uint8_t i_version, bool b_current_next,
                           uint16_t i_pcr_pid)
 * \brief Initialize a user-allocated dvbpsi_pmt_t structure.
 * \param p_pmt pointer to the PMT structure
 * \param i_program_number program number
 * \param i_version PMT version
 * \param b_current_next current next indicator
 * \param i_pcr_pid PCR_PID
 * \return nothing.
 */
void dvbpsi_pmt_init(dvbpsi_pmt_t* p_pmt, uint16_t i_program_number,
                    uint8_t i_version, bool b_current_next, uint16_t i_pcr_pid);


/*****************************************************************************
 * dvbpsi_pmt_empty/dvbpsi_pmt_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_pmt_empty(dvbpsi_pmt_t* p_pmt)
 * \brief Clean a dvbpsi_pmt_t structure.
 * \param p_pmt pointer to the PMT structure
 * \return nothing.
 */
void dvbpsi_pmt_empty(dvbpsi_pmt_t* p_pmt);

/*****************************************************************************
 * dvbpsi_pmt_descriptor_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t* dvbpsi_pmt_descriptor_add(dvbpsi_pmt_t* p_pmt,
                                                    uint8_t i_tag,
                                                    uint8_t i_length,
                                                    uint8_t* p_data)
 * \brief Add a descriptor in the PMT.
 * \param p_pmt pointer to the PMT structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t* dvbpsi_pmt_descriptor_add(dvbpsi_pmt_t* p_pmt,
                                             uint8_t i_tag, uint8_t i_length,
                                             uint8_t* p_data);

/*****************************************************************************
 * dvbpsi_pmt_es_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_pmt_es_t* dvbpsi_pmt_es_add(dvbpsi_pmt_t* p_pmt,
                                        uint8_t i_type, uint16_t i_pid)
 * \brief Add an ES in the PMT.
 * \param p_pmt pointer to the PMT structure
 * \param i_type type of ES
 * \param i_pid PID of the ES
 * \return a pointer to the added ES.
 */
dvbpsi_pmt_es_t* dvbpsi_pmt_es_add(dvbpsi_pmt_t* p_pmt,
                                 uint8_t i_type, uint16_t i_pid);

/*****************************************************************************
 * dvbpsi_pmt_es_descriptor_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t* dvbpsi_pmt_es_descriptor_add(dvbpsi_pmt_es_t* p_es,
                                                      uint8_t i_tag,
                                                      uint8_t i_length,
                                                      uint8_t* p_data)
 * \brief Add a descriptor in the PMT ES.
 * \param p_es pointer to the ES structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t* dvbpsi_pmt_es_descriptor_add(dvbpsi_pmt_es_t* p_es,
                                               uint8_t i_tag, uint8_t i_length,
                                               uint8_t* p_data);

/*****************************************************************************
 * dvbpsi_pmt_sections_generate
 *****************************************************************************/
/*!
 * \fn dvbpsi_psi_section_t* dvbpsi_pmt_sections_generate(dvbpsi_t *p_dvbpsi,
                                                   dvbpsi_pmt_t* p_pmt)
 * \brief PMT generator
 * \param p_dvbpsi handle to dvbpsi with attached decoder
 * \param p_pmt PMT structure
 * \return a pointer to the list of generated PSI sections.
 *
 * Generate PMT sections based on the dvbpsi_pmt_t structure.
 */
dvbpsi_psi_section_t* dvbpsi_pmt_sections_generate(dvbpsi_pmt_t* p_pmt);

#endif /* !VLC_PES_H_ */

