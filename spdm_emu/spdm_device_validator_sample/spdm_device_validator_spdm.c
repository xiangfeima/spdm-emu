/**
 *  Copyright Notice:
 *  Copyright 2021-2022 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/spdm-emu/blob/main/LICENSE.md
 **/

#include "spdm_device_validator_sample.h"

void *m_spdm_context;
void *m_scratch_buffer;
SOCKET m_socket;
extern FILE *m_log_file; 

bool communicate_platform_data(SOCKET socket, uint32_t command,
                               const uint8_t *send_buffer, size_t bytes_to_send,
                               uint32_t *response,
                               size_t *bytes_to_receive,
                               uint8_t *receive_buffer)
{
    bool result;

    result =
        send_platform_data(socket, command, send_buffer, bytes_to_send);
    if (!result) {
        printf("send_platform_data Error - %x\n",
#ifdef _MSC_VER
               WSAGetLastError()
#else
               errno
#endif
               );
        return result;
    }

    result = receive_platform_data(socket, response, receive_buffer,
                                   bytes_to_receive);
    if (!result) {
        printf("receive_platform_data Error - %x\n",
#ifdef _MSC_VER
               WSAGetLastError()
#else
               errno
#endif
               );
        return result;
    }
    return result;
}

libspdm_return_t spdm_device_send_message(void *spdm_context,
                                          size_t request_size, const void *request,
                                          uint64_t timeout)
{
    bool result;

    result = send_platform_data(m_socket, SOCKET_SPDM_COMMAND_NORMAL,
                                request, (uint32_t)request_size);
    if (!result) {
        printf("send_platform_data Error - %x\n",
#ifdef _MSC_VER
               WSAGetLastError()
#else
               errno
#endif
               );
        return LIBSPDM_STATUS_SEND_FAIL;
    }
    return LIBSPDM_STATUS_SUCCESS;
}

libspdm_return_t spdm_device_receive_message(void *spdm_context,
                                             size_t *response_size,
                                             void **response,
                                             uint64_t timeout)
{
    bool result;
    uint32_t command;

    result = receive_platform_data(m_socket, &command, *response,
                                   response_size);
    if (!result) {
        printf("receive_platform_data Error - %x\n",
#ifdef _MSC_VER
               WSAGetLastError()
#else
               errno
#endif
               );
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }
    return LIBSPDM_STATUS_SUCCESS;
}

libspdm_return_t spdm_device_send_message_pci_doe_ssd(void *spdm_context,
                                                      size_t request_size,
                                                      const void *request,
                                                      uint64_t timeout)
{
    /* check PCI-DOE data alignment */
    if (request_size % sizeof(uint32_t) != 0)
    {
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }
    if (m_log_file) fprintf(m_log_file, "send-doe (%ld bytes)...\n", request_size);

    /* write request to PCI-DOE Write Mail Box Register */
    uint32_t* data = (uint32_t*)request;
    size_t offset = 0;
    while (offset < request_size) {
        m_pci_doe_reg_addr->wmb.value = *data;
        if (m_log_file) fprintf(m_log_file, "\t -> 0x%08lX : %08X\n", offset, *data);
        data += 1;
        offset += sizeof(uint32_t);
    }
    /* write PCI-DOE Ctrl Register GO flag */
    m_pci_doe_reg_addr->ctrl.native.go = 0x1;
    if (m_log_file) fprintf(m_log_file, "send-doe ... done\n");

    append_pcap_packet_data(NULL, 0, request,
                                request_size);

    return LIBSPDM_STATUS_SUCCESS;
}

libspdm_return_t spdm_device_receive_message_pci_doe_ssd(void *spdm_context,
                                                         size_t *response_size,
                                                         void **response,
                                                         uint64_t timeout)
{
    while (m_pci_doe_reg_addr->status.native.data_obj_ready != 0x1 && m_pci_doe_reg_addr->status.native.error == 0) ;
    /* check PCI-DOE Status Register data obj ready flag */
    if (!(m_pci_doe_reg_addr->status.native.data_obj_ready == 0x1))
    {
        return LIBSPDM_STATUS_RECEIVE_FAIL;
    }

    if (m_log_file) fprintf(m_log_file, "recv-doe ...\n");
    /* read response data from PCI-DOE Read Mail Box Register */
    uint32_t* data = *response;
    *response_size = 0;
    while (m_pci_doe_reg_addr->status.native.data_obj_ready == 0x1) {
        *data = m_pci_doe_reg_addr->rmb.value;
        if (m_log_file) fprintf(m_log_file, "\t <- 0x%08lX : %08X\n", *response_size, *data);
        data += 1;
        *response_size += sizeof(uint32_t);
        m_pci_doe_reg_addr->rmb.value = 0;
    }
    if (m_log_file) fprintf(m_log_file, "recv-doe ... (%ld bytes) done\n", *response_size);

    append_pcap_packet_data(NULL, 0, *response,
                                *response_size);

    return LIBSPDM_STATUS_SUCCESS;
}

/**
 * Send and receive an DOE message
 *
 * @param request                       the PCI DOE request message, start from pci_doe_data_object_header_t.
 * @param request_size                  size in bytes of request.
 * @param response                      the PCI DOE response message, start from pci_doe_data_object_header_t.
 * @param response_size                 size in bytes of response.
 *
 * @retval LIBSPDM_STATUS_SUCCESS               The request is sent and response is received.
 * @return ERROR                        The response is not received correctly.
 **/
libspdm_return_t pci_doe_send_receive_data(const void *pci_doe_context,
                                           size_t request_size, const void *request,
                                           size_t *response_size, void *response)
{
    if (m_use_transport_layer == TRANSPORT_TYPE_PCI_DOE_SSD) {
        libspdm_return_t status;
        status = spdm_device_send_message_pci_doe_ssd(NULL, request_size, request, 0);
        if (status != LIBSPDM_STATUS_SUCCESS) {
            return status;
        }

        status = spdm_device_receive_message_pci_doe_ssd(NULL, response_size, &response, 0);
        if (status != LIBSPDM_STATUS_SUCCESS) {
            return status;
        }

        return LIBSPDM_STATUS_SUCCESS;
    } else {
        bool result;
        uint32_t response_code;

        result = communicate_platform_data(
            m_socket, SOCKET_SPDM_COMMAND_NORMAL,
            request, request_size,
            &response_code, response_size,
            response);
        if (!result) {
            return LIBSPDM_STATUS_RECEIVE_FAIL;
        }
        return LIBSPDM_STATUS_SUCCESS;
    }
}

void *spdm_client_init(void)
{
    void *spdm_context;
    size_t scratch_buffer_size;

    printf("context_size - 0x%x\n", (uint32_t)libspdm_get_context_size());

    m_spdm_context = (void *)malloc(libspdm_get_context_size());
    if (m_spdm_context == NULL) {
        return NULL;
    }
    spdm_context = m_spdm_context;
    libspdm_init_context(spdm_context);

    if (m_use_transport_layer == TRANSPORT_TYPE_PCI_DOE_SSD) {
        libspdm_register_device_io_func(spdm_context, spdm_device_send_message_pci_doe_ssd,
                                        spdm_device_receive_message_pci_doe_ssd);
    } else {
        libspdm_register_device_io_func(spdm_context, spdm_device_send_message,
                                        spdm_device_receive_message);
    }

    if (m_use_transport_layer == SOCKET_TRANSPORT_TYPE_MCTP) {
        libspdm_register_transport_layer_func(
            spdm_context,
            LIBSPDM_MAX_SPDM_MSG_SIZE,
            LIBSPDM_TRANSPORT_HEADER_SIZE,
            LIBSPDM_TRANSPORT_TAIL_SIZE,
            libspdm_transport_mctp_encode_message,
            libspdm_transport_mctp_decode_message);
    } else if (m_use_transport_layer == SOCKET_TRANSPORT_TYPE_PCI_DOE ||
               m_use_transport_layer == TRANSPORT_TYPE_PCI_DOE_SSD) {
        libspdm_register_transport_layer_func(
            spdm_context,
            LIBSPDM_MAX_SPDM_MSG_SIZE,
            LIBSPDM_TRANSPORT_HEADER_SIZE,
            LIBSPDM_TRANSPORT_TAIL_SIZE,
            libspdm_transport_pci_doe_encode_message,
            libspdm_transport_pci_doe_decode_message);
    } else if (m_use_transport_layer == SOCKET_TRANSPORT_TYPE_NONE) {
        libspdm_register_transport_layer_func(
            spdm_context,
            LIBSPDM_MAX_SPDM_MSG_SIZE,
            0,
            0,
            spdm_transport_none_encode_message,
            spdm_transport_none_decode_message);
    } else {
        free(m_spdm_context);
        m_spdm_context = NULL;
        return NULL;
    }

    libspdm_register_device_buffer_func(spdm_context,
                                        LIBSPDM_SENDER_BUFFER_SIZE,
                                        LIBSPDM_RECEIVER_BUFFER_SIZE,
                                        spdm_device_acquire_sender_buffer,
                                        spdm_device_release_sender_buffer,
                                        spdm_device_acquire_receiver_buffer,
                                        spdm_device_release_receiver_buffer);

    scratch_buffer_size = libspdm_get_sizeof_required_scratch_buffer(m_spdm_context);
    m_scratch_buffer = (void *)malloc(scratch_buffer_size);
    if (m_scratch_buffer == NULL) {
        free(m_spdm_context);
        m_spdm_context = NULL;
        return NULL;
    }
    libspdm_set_scratch_buffer (spdm_context, m_scratch_buffer, scratch_buffer_size);

    return m_spdm_context;
}
