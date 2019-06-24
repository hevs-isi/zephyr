
//------------------------------------------------------------------------------
//
// Include Files
//
//------------------------------------------------------------------------------

#include "wimod_hci_driver.h"

#include <crc.h>
#include "wimod_slip.h"
#include <uart.h>

#include <string.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(lw_hci, LOG_LEVEL_DBG);

static struct device *uart_dev;

#define CRC16_INIT_VALUE	0xFFFF	// initial value for CRC algorithem
#define CRC16_GOOD_VALUE	0x0F47	// constant compare value for check

static bool CRC16_Check (const u8_t* data, u16_t length, u16_t initVal)
{
	// calc ones complement of CRC16
	u16_t crc = ~crc16_ccitt(CRC16_INIT_VALUE, data, length);

	// CRC ok ?
	return (bool)(crc == CRC16_GOOD_VALUE);
}

//------------------------------------------------------------------------------
//
//  Forward Declaration
//
//------------------------------------------------------------------------------

// SLIP Message Receiver Callback
static u8_t *wimod_hci_process_rx_message(u8_t* rx_data, int rx_length);

//------------------------------------------------------------------------------
//
// Declare Layer Instance
//
//------------------------------------------------------------------------------

typedef struct
{
	// CRC Error counter
	u32_t crc_errors;

	// rx_message
	wimod_hci_message_t* rx_message;

	// Receiver callback
	wimod_hci_cb_rx_message  cb_rx_message;

} wimod_hci_msglayer_t;

//------------------------------------------------------------------------------
//
//  Section RAM
//
//------------------------------------------------------------------------------

// reserve HCI Instance
static wimod_hci_msglayer_t  HCI;

// reserve one tx_buffer
static u8_t tx_buffer[sizeof( wimod_hci_message_t ) * 2 + 2];

//------------------------------------------------------------------------------
//
//  Init
//
//  @brief: Init HCI Message layer
//
//------------------------------------------------------------------------------

static void uart_isr(struct device *dev)
{
	//uart_irq_update(dev);
	static int i;

	while (uart_irq_update(dev) &&
			uart_irq_is_pending(dev)) {

			u8_t byte;
			int rx;

			if (!uart_irq_rx_ready(dev)) {
				continue;
			}

			rx = uart_fifo_read(dev, &byte, 1);
			if (rx < 0) {
				return;
			}
			slip_decode_data(&byte, 1);
			i++;
	}

	//got = 1;

/*
	if (uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			got = uart_fifo_read(uart_dev, rx_buffer, 1);
			if (got <= 0) {
				return;
			}
		}
	}
*/
}

int wimod_hci_init(wimod_hci_cb_rx_message   cb_rx_message,
					wimod_hci_message_t*	rx_message)
{

	u8_t c;
	// init error counter
	HCI.crc_errors = 0;

	// save receiver callback
	HCI.cb_rx_message = cb_rx_message;

	// save rx_message
	HCI.rx_message = rx_message;


	// init SLIP
	slip_init(wimod_hci_process_rx_message);

	// init first RxBuffer to SAP_ID of HCI message, size without 16-Bit length Field
	slip_set_rx_buffer(&rx_message->sap_id, sizeof(wimod_hci_message_t) - sizeof(u16_t));

	//uart_pipe_register(rxsplipbuf, sizeof(rxsplipbuf), upipe_rx);
	uart_dev = device_get_binding(CONFIG_LORA_IM881A_UART_DRV_NAME);

	if (!uart_dev) {
		return -ENODEV;
	}

	uart_irq_rx_disable(uart_dev);
	//uart_irq_tx_disable(uart_dev);

	/* Drain the fifo */
	while (uart_irq_rx_ready(uart_dev)) {
		uart_fifo_read(uart_dev, &c, 1);
	}

	uart_irq_callback_set(uart_dev, uart_isr);

	uart_irq_rx_enable(uart_dev);

	return 0;
}

//------------------------------------------------------------------------------
//
//  SendMessage
//
//  @brief: Send a HCI message (with or without payload)
//
//------------------------------------------------------------------------------

int wimod_hci_send_message(wimod_hci_message_t* tx_message)
{
	LOG_DBG("here");

	u8_t buf[1] = { SLIP_END };
	u8_t i;

	// 1. check parameter
	//
	// 1.1 check ptr
	//
	if (!tx_message)
	{
		// error
		return -1;
	}

	// 2. Calculate CRC16 over header and optional payload
	//

	u16_t crc16 = crc16_ccitt(CRC16_INIT_VALUE, &tx_message->sap_id,
		tx_message->length + WIMOD_HCI_MSG_HEADER_SIZE);

	// 2.1 get 1's complement !!!
	//
	crc16 = ~crc16;

	// 2.2 attach CRC16 and correct length, LSB first
	//
	tx_message->payload[tx_message->length]	= LOBYTE(crc16);
	tx_message->payload[tx_message->length + 1] = HIBYTE(crc16);

	// 3. perform SLIP encoding
	//	- start transmission with SAP ID
	//	- correct length by header size

	int tx_length = slip_encode_data(tx_buffer,
								sizeof(tx_buffer),
								&tx_message->sap_id,
								tx_message->length + WIMOD_HCI_MSG_HEADER_SIZE + WIMOD_HCI_MSG_FCS_SIZE);
	// message ok ?
	if (!(tx_length > 0))
	{
		return -1;
	}

	// send wakeup chars
	for(i= 0; i < 40; i++){
		//uart_pipe_send(&buf[0], 1);
		uart_poll_out(uart_dev, buf[0]);
	}

	// 4. send octet sequence over serial device
	for(i= 0; i < tx_length; i++){
		//uart_pipe_send(&buf[0], 1);
		uart_poll_out(uart_dev, tx_buffer[i]);
	}

	return 0;
}

//------------------------------------------------------------------------------
//
//  WiMOD_HCI_Processrx_message
//
//  @brief: process received SLIP message and return new rx_buffer
//
//------------------------------------------------------------------------------

static u8_t* wimod_hci_process_rx_message(u8_t* rx_data, int rx_length)
{
	LOG_DBG("wimod_hci_process_rx_message");
	// check min length
	if (rx_length >= (WIMOD_HCI_MSG_HEADER_SIZE + WIMOD_HCI_MSG_FCS_SIZE))
	{
		if (CRC16_Check(rx_data, rx_length, CRC16_INIT_VALUE))
		{
			// receiver registered ?
			if (HCI.cb_rx_message)
			{
				// yes, complete message info
				HCI.rx_message->length = rx_length - (WIMOD_HCI_MSG_HEADER_SIZE + WIMOD_HCI_MSG_FCS_SIZE);

				// call upper layer receiver and save new rx_message
				HCI.rx_message = (*HCI.cb_rx_message)(HCI.rx_message);
			}
		}
		else
		{
			HCI.crc_errors++;
		}
	}

	// free HCI message available ?
	if (HCI.rx_message)
	{
		// yes, return pointer to first byte
		return &HCI.rx_message->sap_id;
	}

	// error, disable SLIP decoder
	return 0;
}
