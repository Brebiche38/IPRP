#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <linux/ip.h>

#include "../../inc/ird.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

extern list_t receiver_links;

/**
Look for the SNSID of the given IPRP header in the list of links

\param header The IPRP header we're looking for
\return The corresponding receiver link, or NULL if none found
*/
iprp_receiver_link_t *receiver_link_get(iprp_header_t *header) {
	iprp_receiver_link_t *packet_link = NULL;

	// Look for SNSID in known receiver links
	list_elem_t *iterator = receiver_links.head;
	while(iterator != NULL) {
		// Compare SNSIDs
		iprp_receiver_link_t *link = (iprp_receiver_link_t *) iterator->elem;
		bool same = true;
		for (int i = 0; i < IPRP_SNSID_SIZE; ++i) {
			if (link->snsid[i] != header->snsid[i]) {
				same = false;
				break;
			}
		}

		// Found the link
		if (same) {
			packet_link = link;
			break;
		}

		iterator = iterator->next;
	}

	return packet_link;
}

/**
Create a receiver link structure with the given IPRP header.

\param header The header containing information to create the link
\return On success, the link is returned. On failure, NULL is returned and errno is set accordingly.
*/
iprp_receiver_link_t *receiver_link_create(iprp_header_t *header) {
	iprp_receiver_link_t *packet_link = malloc(sizeof(iprp_receiver_link_t));
	if (!packet_link) {
		return NULL;
	}

	memcpy(&packet_link->src_addr, &header->snsid, sizeof(struct in_addr));
	memcpy(&packet_link->src_port, &header->snsid[16], sizeof(uint16_t));
	packet_link->src_port = ntohs(packet_link->src_port);
	memcpy(&packet_link->snsid, &header->snsid, 20);

	for (int i = 0; i < IPRP_DD_MAX_LOST_PACKETS; ++i) {
		packet_link->list_sn[i] = 0;
	}
	packet_link->high_sn = header->seq_nb;
	packet_link->last_seen = time(NULL);

	return packet_link;
}

/**
Duplicate discard algorithm

The algorithm determines if the given packet should be forwarded to the application or dropped

\param packet The IPRP header of the packet to check
\param link The receiver link structure containing the state information about the connection
\return Whether the packet has to be forwarded or not
*/
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

/**
Computes the IP checksum from an IP header

\param header The IP header, with its checksum field set to 0
\param len The length of the IP header in bytes
\return The IP checksum
*/
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

/**
Computes the UDP checksum from a UDP header and the given IP pseudo-header

\param packet The entire UDP payload, with UDP ckhecksum field set to 0
\param len The length of the payload in bytes
\param src_addr The source IP address for the pseudo-header
\param dest_addr The destination IP address for the pseudo-header
\return The IP checksum
*/
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr) {
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