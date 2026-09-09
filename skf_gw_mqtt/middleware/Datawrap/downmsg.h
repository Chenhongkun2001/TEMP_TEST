#ifndef DOWNMSG_H
#define DOWNMSG_H
#include "common.h"
#include "datawrap.h"

void Simple_retrive(pb_size_t which_msg, pb_istream_t *stream);
void Simple_disseminate(pb_size_t which_msg, pb_istream_t *stream);
void Simple_bulk_disseminate(pb_size_t which_msg, pb_istream_t *stream);
void Reliable_disseminate(pb_size_t which_msg, pb_istream_t *stream);
#endif
