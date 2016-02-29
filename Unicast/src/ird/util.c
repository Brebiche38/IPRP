#include <stdbool.h>
#include <stdio.h>

#include "../../inc/ird.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link) {
	// TODO resetCtr doesn't make sense...
	if (packet->seq_nb == link->high_sn) {
		// Duplicate packet
		return false;
	} else {
		if (packet->seq_nb > link->high_sn) {
			// Fresh packet out of order
			// We lose space for received packets (we can accept very late packets although more recent ones would be dropped)
			for (int i = link->high_sn + 1; i < packet->seq_nb; ++i) {
				link->list_sn[i % IPRP_DD_MAX_LOST_PACKETS] = i; // TODO does this really work? What about earlier lost packets?
			}
			link->high_sn = packet->seq_nb;
			//*resetCtr = 0; // TODO in original code, modifies last_seen (highTime)
			return 1;
		}
		else
		{
			if (packet->seq_nb < link->high_sn && link->high_sn - packet->seq_nb > IPRP_DD_MAX_LOST_PACKETS) {
				printf("Very Late Packet\n");
				//*resetCtr = *resetCtr + 1;
			}
			/*if(*resetCtr ==MAX_OLD)
			{
				printf("Reboot Detected\n");
				*highSN = 0;
				*resetCtr = 0;
				for(ii = 0; ii < MAX_LOST; ii++)
					listSN[ii] = -1;
				return 1;
			}*/

			if (link->list_sn[packet->seq_nb % IPRP_DD_MAX_LOST_PACKETS] == packet->seq_nb) {
				// The sequence number is in the list, it is a late packet
				//Remove from List
				link->list_sn[packet->seq_nb % IPRP_DD_MAX_LOST_PACKETS] = 0;
				return true;
			} else {
				return false;
			}
		}
	}
}

uint16_t ip_checksum(struct iphdr *header, size_t len) {
	uint32_t checksum = 0;
	uint16_t *halfwords = (uint16_t *) header;

	for (int i = 0; i < len/2; ++i) {
		checksum += halfwords[i];
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}
	
	while (checksum >> 16) {
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}

	return (uint16_t) ~checksum;
}

uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr) {
	// TODO do
	uint32_t checksum = 0;

	// Pseudo-header
	checksum += (((uint16_t *) src_addr)[0]);
	checksum += (((uint16_t *) src_addr)[1]);

	checksum += (((uint16_t *) dest_addr)[0]);
	checksum += (((uint16_t *) dest_addr)[1]);

	checksum += htons(IPPROTO_UDP);
	checksum += htons(len);

	while(checksum >> 16) {
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}

	// Calculate the sum
	for (int i = 0; i < len/2; ++i) {
		checksum += packet[i];
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}

	if (len % 2 == 1) {
		checksum += *((uint8_t *) packet[len/2]);
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}

	return (uint16_t) ~checksum;
}