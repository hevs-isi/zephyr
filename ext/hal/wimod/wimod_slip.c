//------------------------------------------------------------------------------
//
//  Include Files
//
//------------------------------------------------------------------------------

#include <kernel.h>
#include "wimod_slip.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(lw_slip, LOG_LEVEL_INF);

//------------------------------------------------------------------------------
//
//  Protocol Definitions
//
//------------------------------------------------------------------------------

// slip Receiver/Decoder States
#define SLIPDEC_IDLE_STATE			0
#define	SLIPDEC_START_STATE			1
#define	SLIPDEC_IN_FRAME_STATE		2
#define	SLIPDEC_ESC_STATE			3

//------------------------------------------------------------------------------
//
// Declare slip Variables
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
// Section RAM
//
//------------------------------------------------------------------------------

// slip Instance

//------------------------------------------------------------------------------
//
// Section Code
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  Init
//
//  @brief: init slip decoder
//
//------------------------------------------------------------------------------

void slip_init(struct slip_t *slip, slip_cb_rx_message_t cb_rx_message) {
	// init decoder to idle state, no rx-buffer avaliable
	slip->rx_state = SLIPDEC_IDLE_STATE;
	slip->rx_index = 0;
	slip->rx_buffer = 0;
	slip->rx_buf_size = 0;

	// save message receiver callback
	slip->cb_rx_message = cb_rx_message;
}

struct slip_tx_t
{
	int tx_index;
	int tx_buf_size;
	u8_t *tx_buffer;
};

static void slip_store_tx_byte(struct slip_tx_t *slip, u8_t tx_byte) {
	if (slip->tx_index < slip->tx_buf_size)
		slip->tx_buffer[slip->tx_index++] = tx_byte;
}

int slip_encode_data(u8_t *dst_buffer, int dst_buf_size, const u8_t *src_data,
		int src_length) {
	// init tx_buffer
	struct slip_tx_t slip;
	slip.tx_buffer = dst_buffer;

	// init tx_index
	slip.tx_index = 0;

	// init size
	slip.tx_buf_size = dst_buf_size;

	// send start of slip message
	slip_store_tx_byte(&slip, SLIP_END);

	// iterate over all message bytes
	while (src_length--) {
		switch (*src_data) {
		case SLIP_END:
			slip_store_tx_byte(&slip, SLIP_ESC);
			slip_store_tx_byte(&slip, SLIP_ESC_END);
			break;

		case SLIP_ESC:
			slip_store_tx_byte(&slip, SLIP_ESC);
			slip_store_tx_byte(&slip, SLIP_ESC_ESC);
			break;

		default:
			slip_store_tx_byte(&slip, *src_data);
			break;
		}
		// next byte
		src_data++;
	}

	// send end of slip message
	slip_store_tx_byte(&slip, SLIP_END);

	// length ok ?
	if (slip.tx_index <= slip.tx_buf_size)
		return slip.tx_index;

	// return tx length error
	return -1;
}

//------------------------------------------------------------------------------
//
//  SetRxBuffer
//
//  @brief: configure a rx-buffer and enable receiver/decoder
//
//------------------------------------------------------------------------------

bool slip_set_rx_buffer(struct slip_t *slip, u8_t *rx_buffer, int rx_buf_size) {
	// receiver in IDLE state and client already registered ?
	if ((slip->rx_state == SLIPDEC_IDLE_STATE) && slip->cb_rx_message) {
		// same buffer params
		slip->rx_buffer = rx_buffer;
		slip->rx_buf_size = rx_buf_size;

		// enable decoder
		slip->rx_state = SLIPDEC_START_STATE;

		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
//
//  slip_store_rx_byte
//
//  @brief: store slip decoded rx_byte
//
//------------------------------------------------------------------------------

static void slip_store_rx_byte(struct slip_t *slip, u8_t rx_byte) {
	if (slip->rx_index < slip->rx_buf_size)
		slip->rx_buffer[slip->rx_index++] = rx_byte;
	else
		LOG_WRN("full");
}

//------------------------------------------------------------------------------
//
//  DecodeData
//
//  @brief: process received byte stream
//
//------------------------------------------------------------------------------

void slip_decode_data(struct slip_t *slip, const uint8_t *src_data, int src_length) {
	// iterate over all received bytes
	LOG_DBG("data:0x%02"PRIx8, *src_data);
	while (src_length--) {
		// get rx_byte
		u8_t rx_byte = *src_data++;

		// decode according to current state
		switch (slip->rx_state) {
		case SLIPDEC_START_STATE:
			// start of slip frame ?
			if (rx_byte == SLIP_END) {
				// init read index
				slip->rx_index = 0;

				// next state
				slip->rx_state = SLIPDEC_IN_FRAME_STATE;
			}
			break;

		case SLIPDEC_IN_FRAME_STATE:
			switch (rx_byte) {
			case SLIP_END:
				// data received ?
				if (slip->rx_index > 0) {
					// yes, receiver registered ?
					if (slip->cb_rx_message) {
						// yes, call message receive
						slip->rx_buffer = (*slip->cb_rx_message)(slip->rx_buffer,
								slip->rx_index);

						// new buffer available ?
						if (!slip->rx_buffer) {
							slip->rx_state = SLIPDEC_IDLE_STATE;
						} else {
							slip->rx_state = SLIPDEC_START_STATE;
						}
					} else {
						// disable decoder, temp. no buffer avaliable
						slip->rx_state = SLIPDEC_IDLE_STATE;
					}
				}
				// init read index
				slip->rx_index = 0;
				break;

			case SLIP_ESC:
				// enter escape sequence state
				slip->rx_state = SLIPDEC_ESC_STATE;
				break;

			default:
				// store byte
				slip_store_rx_byte(slip, rx_byte);
				break;
			}
			break;

		case SLIPDEC_ESC_STATE:
			switch (rx_byte) {
			case SLIP_ESC_END:
				slip_store_rx_byte(slip, SLIP_END);
				// quit escape sequence state
				slip->rx_state = SLIPDEC_IN_FRAME_STATE;
				break;

			case SLIP_ESC_ESC:
				slip_store_rx_byte(slip, SLIP_ESC);
				// quit escape sequence state
				slip->rx_state = SLIPDEC_IN_FRAME_STATE;
				break;

			default:
				// abort frame receiption
				slip->rx_state = SLIPDEC_START_STATE;
				break;
			}
			break;

		default:
			break;
		}
	}
}
