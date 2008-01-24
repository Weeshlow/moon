/*
 * asf.cpp: 
 *
 * Author: Rolf Bjarne Kvinge  (RKvinge@novell.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 */
#include <config.h>
#include "asf.h"
#include "../debug.h"


/*
 * ASFMediaSource
 */

ASFMediaSource::ASFMediaSource (ASFParser *parser, IMediaSource *source) : ASFSource (parser)
{
	this->source = source;
}

bool 
ASFMediaSource::ReadInternal (void *buf, uint32_t n)
{
	return source->ReadAll (buf, n);
}

bool 
ASFMediaSource::SeekInternal (int64_t offset, int mode)
{
	return source->Seek (offset, mode);
}

int64_t
ASFMediaSource::Position ()
{
	return source->GetPosition ();
}

bool 
ASFMediaSource::CanSeek ()
{
	return source->IsSeekable ();
}

bool 
ASFMediaSource::Eof ()
{
	return source->Eof ();
}

/*
	ASFParser
*/

ASFParser::ASFParser (ASFSource *src)
{
	ASF_LOG ("ASFParser::ASFParser ('%p'), this: %p.\n", src, this);
	source = src;
	Initialize ();
}

uint64_t
ASFParser::GetPacketCount ()
{
	return file_properties->data_packet_count;
}

void
ASFParser::Initialize ()
{
	header = NULL;
	data = NULL;
	data_offset = 0;
	packet_offset = 0;
	packet_offset_end = 0;
	header_objects = NULL;
	header_extension = NULL;
	script_command = NULL;
	marker = NULL;
	file_properties = NULL;
	errors = NULL;
	embedded_script_command = NULL;
	embedded_script_command_state = NULL;
	memset (stream_properties, 0, sizeof (asf_stream_properties *) * 127);
}

ASFParser::~ASFParser ()
{
	ASF_LOG ("ASFParser::~ASFParser ().\n");
	
	delete source;
		
	if (errors) {
		error *next;
		
		while (errors != NULL) {
			next = errors->next;
			g_free (errors->msg);
			g_free (errors);
			errors = next;
		}
	}
	
	g_free (header);
	g_free (data);
	
	if (header_objects) {
		for (int i = 0; header_objects[i]; i++)
			g_free (header_objects[i]);
		
		g_free (header_objects);
	}
}

bool
ASFParser::VerifyHeaderDataSize (uint32_t size)
{
	if (header == NULL)
		return false;
	
	return size >= 0 && size < header->size;
}

void *
ASFParser::MallocVerified (uint32_t size)
{
	void *result = g_try_malloc0 (size);
	
	if (result == NULL)
		AddError ("Out of memory.");
	
	return result;
}

void *
ASFParser::Malloc (uint32_t size)
{
	if (!VerifyHeaderDataSize (size))
		return NULL;
	
	return MallocVerified (size);
}

asf_object *
ASFParser::ReadObject (asf_object *obj)
{
	ASFTypes type = asf_get_guid_type (&obj->id);
	asf_object *result = NULL;
	char *guid;
	
	ASF_LOG ("ASFParser::ReadObject ('%s', %llu)\n", asf_guid_tostring (&obj->id), obj->size);
	
	if (type == ASF_NONE) {
		guid = asf_guid_tostring (&obj->id);
		AddError (g_strdup_printf ("Unrecognized guid: %s.", guid));
		g_free (guid);
		return NULL;
	}
	
	result = (asf_object *) Malloc (obj->size);
	if (result == NULL) {
		guid = asf_guid_tostring (&obj->id);
		AddError (g_strdup_printf ("Header corrupted (id: %s)", guid));
		g_free (guid);
		return false;
	}
	
	memcpy (result, obj, sizeof (asf_object));
	
	if (obj->size > sizeof (asf_object)) {
		if (!source->Read (((char *) result) + sizeof (asf_object), obj->size - sizeof (asf_object))) {
			g_free (result);
			return NULL;
		}
	}
	
	if (!asf_object_validate_exact (result, this)) {
		g_free (result);
		return NULL;
	}
	
	return result;
}

bool
ASFParser::ReadPacket (ASFPacket *packet, int packet_index)
{
	ASF_LOG ("ASFParser::ReadPacket (%p, %d).\n", packet, packet_index);
	
	if (packet_index >= 0) {
		int64_t position = GetPacketOffset (packet_index);
		
		if (position == 0 || (source->Position () != position && !source->Seek (position, SEEK_SET)))
			return false;
	}
	
	return ASFParser::ReadPacket (packet);
}

bool
ASFParser::ReadPacket (ASFPacket *packet)
{
	asf_payload_parsing_information ppi;
	asf_error_correction_data ecd;
	
	ASF_LOG ("ASFParser::ReadPacket (%p): Reading packet at %lld (index: %d) of %lld packets.\n",
		 packet, source->Position (), GetPacketIndex (source->Position ()),
		 data->data_packet_count);
	
	if (!ecd.FillInAll (this))
		return false;
	
	asf_error_correction_data_dump (&ecd);
	
	if (!ppi.FillInAll (this))
		return false;
	
	asf_payload_parsing_information_dump (&ppi);
	
	asf_multiple_payloads *mp = new asf_multiple_payloads ();
	if (!mp->FillInAll (this, &ecd, ppi)) {
		delete mp;
		return false;
	}

	packet->payloads = mp;
	
/*	ASF_LOG ("ASFParser::ReadPacket (): Current position (end of packet): %llx (%lld), start position was: %llx (%lld), difference: %llx (%lld)\n", 
		source->Position (), source->Position (), 
		start_position, start_position,
		source->Position () - start_position, source->Position () - start_position);
	*/
	return true;
}

bool
ASFParser::ReadData ()
{
	ASF_LOG ("ASFParser::ReadData ().\n");
	
	if (this->data != NULL) {
		AddError ("ReadData has already been called.\n");
		return false;
	}
	
	if (!source->Seek ((int64_t) header->size, SEEK_SET)) {
		return false;
	}
	
	ASF_LOG ("Current position: %llx (%lld)\n", source->Position (), source->Position ());
	
	data = (asf_data *) Malloc (sizeof (asf_data));
	if (data == NULL) {
		AddError ("Data corruption in data.");
		return false;
	}
	
	if (!source->Read (data, sizeof (asf_data))) {
		g_free (data);
		data = NULL;
		return false;
	}
	
	asf_object_dump_exact (data);
	
	ASF_LOG ("Data %p has %lld packets.\n", data, data->data_packet_count);
	
	this->data = data;
	
	return true;
}

bool
ASFParser::ReadHeader ()
{
	ASF_LOG ("ASFParser::ReadHeader ().\n");
	
	header = (asf_header *) MallocVerified (sizeof (asf_header));
	if (header == NULL) {
		ASF_LOG ("ASFParser::ReadHeader (): Malloc failed.\n");
		return false;
	}
	
	if (!source->Read (header, sizeof (asf_header))) {
		ASF_LOG ("ASFParser::ReadHeader (): source->Read () failed.\n");
		return false;
	}
	
	asf_header_dump (header);

	if (!asf_header_validate (header, this)) {
		ASF_LOG ("Header validation failed, error: '%s'\n", GetLastError ());
		return false;
	}
	
	header_objects = (asf_object **) Malloc ((header->object_count + 1) * sizeof (asf_object*));
	if (header_objects == NULL) {
		AddError ("Data corruption in header.");
		return false;
	}
	
	ASF_LOG ("ASFParser::ReadHeader (): about to read streams...\n");
	
	bool any_streams = false;
	for (uint32_t i = 0; i < header->object_count; i++) {
		asf_object tmp;
		
		if (!source->Read (&tmp, sizeof (asf_object)))
			return false;
		
		if (!(header_objects [i] = ReadObject (&tmp)))
			return false;
		
		if (asf_guid_compare (&asf_guids_stream_properties, &header_objects[i]->id)) {
			asf_stream_properties *stream = (asf_stream_properties *) header_objects[i];
			SetStream (stream->get_stream_id (), stream);
			any_streams = true;
		}
		
		if (asf_guid_compare (&asf_guids_file_properties, &header_objects [i]->id)) {
			if (file_properties != NULL) {
				AddError ("Multiple file property object in the asf data.");
				return false;
			}
			file_properties = (asf_file_properties*) header_objects [i];
		}
		
		if (asf_guid_compare (&asf_guids_header_extension, &header_objects [i]->id)) {
			if (header_extension != NULL) {
				AddError ("Multiple header extension objects in the asf data.");
				return false;
			}
			header_extension = (asf_header_extension*) header_objects [i];
		}
		
		if (asf_guid_compare (&asf_guids_marker, &header_objects [i]->id)) {
			if (marker != NULL) {
				AddError ("Multiple marker objects in the asf data.");
				return false;
			}
			marker = (asf_marker*) header_objects [i];
		}
		
		if (asf_guid_compare (&asf_guids_script_command, &header_objects [i]->id)) {
			if (script_command != NULL) {
				AddError ("Multiple script command objects in the asf data.");
				return false;
			}
			script_command = (asf_script_command*) header_objects [i];
		}
		
		asf_object_dump_exact (header_objects [i]);
	}

	// Check for required objects.
	
	if (file_properties == NULL) {
		AddError ("No file property object in the asf data.");
		return false;
	}
	
	if (header_extension == NULL) {
		AddError ("No header extension object in the asf data.");
		return false;
	}
	
	if (!any_streams) {
		AddError ("No streams in the asf data.");
		return false;
	}

	// 
	
	data_offset = header->size;
	packet_offset = data_offset + sizeof (asf_data);
	packet_offset_end = packet_offset + file_properties->data_packet_count * file_properties->min_packet_size;

	ASF_LOG ("ASFParser::ReadHeader (): Header read successfully.\n");
	
	if (!ReadData ())
		return false;
		
	return true;
}

const char *
ASFParser::GetLastError ()
{
	error *last = errors;
	
	while (last != NULL && last->next != NULL)
		last = last->next;
	
	if (last != NULL) {
		return last->msg;
	} else {
		return NULL;
	}
}

void
ASFParser::AddError (const char *msg)
{
	AddError (g_strdup (msg));
}

void
ASFParser::AddError (char *msg)
{
	ASF_LOG ("ASFParser::AddError ('%s').\n", msg);
	
	error *err = (error *) g_malloc0 (sizeof (error));
	err->msg = msg;
	
	error *last = errors;
	while (last != NULL && last->next != NULL)
		last = last->next;
	
	if (last == NULL) {
		errors = err;
	} else {
		last->next = err;
	}
}

asf_stream_properties* 
ASFParser::GetStream (int stream_index)
{
	if (stream_index < 1 || stream_index > 127)
		return NULL;
	
	return stream_properties [stream_index - 1];
}

void
ASFParser::SetStream (int stream_index, asf_stream_properties *stream)
{
	if (stream_index < 1 || stream_index > 127) {
		printf ("ASFParser::SetStream (%i, %p): Invalid stream index.\n", stream_index, stream);
		return;
	}
	
	stream_properties [stream_index - 1] = stream;
}

bool
ASFParser::IsValidStream (int stream_index)
{
	return GetStream (stream_index) != NULL;
}

int64_t
ASFParser::GetPacketOffset (int packet_index)
{
	if (packet_index < 0 || packet_index >= (int) file_properties->data_packet_count) {
//		AddError (g_strdup_printf ("ASFParser::GetPacketOffset (%i): Invalid packet index (there are %llu packets).", packet_index, file_properties->data_packet_count)); 
		return 0;
	}
	
	// CHECK: what if min_packet_size != max_packet_size?
	return packet_offset + packet_index * file_properties->min_packet_size;
}

int
ASFParser::GetPacketIndex (int64_t offset)
{
	if (offset < packet_offset)
		return -1;
	
	if (offset > packet_offset_end)
		return file_properties->data_packet_count;
	
	return (offset - packet_offset) / file_properties->min_packet_size;
}

int
ASFParser::GetPacketIndexOfPts (int stream_id, uint64_t pts)
{
	ASFPacket *packet = NULL;
	int result = 0;
	
	// Read packets until we find the packet which has a pts
	// greater than the one we're looking for.
	
	while (ReadPacket (packet, result)) {
		uint64_t current_pts = packet->GetPts (stream_id);
		
		if (current_pts == ULLONG_MAX) // Can't read pts for some reason.
			return -1;
		
		if (current_pts > pts) // We've found the packet after the one we're looking for
			return result - 1; // return the previous one.
		
		result++;
	}
	
	return -1;
}

asf_header *
ASFParser::GetHeader ()
{
	return header;
}

asf_file_properties *
ASFParser::GetFileProperties ()
{ 
	return file_properties;
}

asf_object *
ASFParser::GetHeaderObject (const asf_guid *guid)
{
	int index = GetHeaderObjectIndex (guid);
	
	if (index >= 0) {
		return header_objects [index];
	} else {
		return NULL;
	}
}

int 
ASFParser::GetHeaderObjectIndex (const asf_guid *guid, int start)
{
	int i = start;
	
	if (i < 0)
		return -1;
		
	while (header_objects [i] != NULL) {
		if (asf_guid_compare (guid, &header_objects [i]->id))
			return i;
	
		i++;
	}
	
	return -1;
}

asf_object *
ASFParser::GetHeader (int index) 
{
	if (index < 0)
		return NULL;
	
	return header_objects [index];
}


/*
	ASFSource
*/

ASFSource::ASFSource (ASFParser *asf_parser)
{
	parser = asf_parser;
}

ASFSource::~ASFSource ()
{
	parser = NULL;
}

bool
ASFSource::Seek (int64_t offset)
{
	return SeekInternal (offset, SEEK_CUR);
}

bool
ASFSource::Seek (int64_t offset, int mode)
{
	return SeekInternal (offset, mode);
}


bool
ASFSource::Read (void *buf, uint32_t n)
{
	return ReadInternal (buf, n);
}

bool 
ASFSource::ReadEncoded (uint32_t length, uint32_t *dest)
{
	uint16_t result2 = 0;
	uint8_t result1 = 0;
	
	switch (length) {
	case 0x00:
		return true;
	case 0x01: 
		if (!Read (&result1, 1))
			return false;
		*dest = result1;
		return true;
	case 0x02:
		if (!Read (&result2, 2))
			return false;
		*dest = result2;
		return true;
	case 0x03:
		return Read (dest, 4);
	default:
		parser->AddError (g_strdup_printf ("Invalid read length: %i.", length));
		return false;
	}
}

/*
	ASFPacket
*/

ASFPacket::ASFPacket ()
{
	payloads = NULL;
	position = -1;
	index = -1;
}

ASFPacket::~ASFPacket ()
{
	delete payloads;
}

int
ASFPacket::GetPayloadCount ()
{
	if (!payloads)
		return 0;
	
	return payloads->get_number_of_payloads ();
}

asf_single_payload *
ASFPacket::GetPayload (int index /* 0 based */)
{
	if (index >= 0 && index < GetPayloadCount ())
		return payloads->payloads [index];
	
	return NULL;
}

uint64_t
ASFPacket::GetPts (int stream_id)
{
	if (!payloads)
		return ULLONG_MAX;
	
	asf_single_payload *first = GetFirstPayload (stream_id);
	if (!first)
		return ULLONG_MAX;
	
	return first->get_presentation_time ();
}

asf_single_payload *
ASFPacket::GetFirstPayload (int stream_id /* 1 - 127 */)
{
	if (!payloads)
		return NULL;
	
	int index = 0;
	while (payloads->payloads [index] != NULL) {
		if (payloads->payloads [index]->stream_id == stream_id)
			return payloads->payloads [index];
		index++;
	}
	
	return NULL;
}

/*
	ASFFrameReader
*/

ASFFrameReader::ASFFrameReader (ASFParser *p)
{
	parser = p;
	first = NULL;
	last = NULL;
	size = 0;
	payloads = NULL;
	current_packet_index = CanSeek () ? 0 : -1;
	
	payloads_size = 0;
	payloads = NULL;
	eof = false;
	
	first_pts = parser->GetFileProperties ()->preroll;
	
	script_command_stream_index = 0;
	FindScriptCommandStream ();
}

ASFFrameReader::~ASFFrameReader ()
{
	RemoveAll ();
	
	for (int i = 0; payloads[i]; i++)
		delete payloads[i];
	
	g_free (payloads);
}

void
ASFFrameReader::FindScriptCommandStream ()
{
	if (script_command_stream_index > 0)
		return;
	
	for (int i = 1; i <= 127; i++) {
		asf_stream_properties* stream = parser->GetStream (i);
		//printf ("Checking guid of stream %i (%p): %s against %s\n", i, stream, stream == NULL ? "-" : asf_guid_tostring (&stream->stream_type), stream == NULL ? "-" : asf_guid_tostring (&asf_guids_media_command));
		if (stream != NULL && asf_guid_compare (&stream->stream_type, &asf_guids_media_command)) {
			script_command_stream_index = i;
			break;
		}
	}
}

bool
ASFFrameReader::ResizeList (int size)
{
	asf_single_payload **new_list = NULL;
	
	if (payloads_size >= size && size > 0)
		return true;
	
	// Allocate a new list
	new_list = (asf_single_payload **) parser->Malloc (sizeof (asf_single_payload*) * (size + 1));
	
	if (new_list == NULL) {
		return false;
	}
	
	if (payloads != NULL) {
		// Copy the old list to the new one
		memcpy (new_list, payloads, payloads_size * sizeof (asf_single_payload*));
		g_free (payloads);
	}
	
	payloads = new_list;
	payloads_size = size;
	
	return true;
}

bool
ASFFrameReader::Seek (int stream_id, uint64_t pts)
{
	//printf ("ASFFrameReader::Seek (%d, %llu).\n", stream_id, pts);
	
	if (!CanSeek ())
		return false;
	
	// Now this is an algorithm that might need some optimization.
	// We seek from the first frame to the key frame AFTER the one we want (counting the numbers of frames)
	// then we seek again from the first frame until the number of frames counted - 1.	
	
	int counter = 0;
	bool found = true; // <- This is broken...
	
	current_packet_index = 0;
	RemoveAll ();
	
	while (Advance (stream_id)) {
		//printf ("ASFFrameReader::Seek (%d, %llu): Checking pts: %llu\n", stream_id, pts, Pts ());
		if (Pts () > pts) {
			//printf ("ASFFrameReader::Seek (%d, %llu): Found pts: %llu\n", stream_id, pts, Pts ());
			if (IsKeyFrame ()) {
				//printf ("ASFFrameReader::Seek (%d, %llu): Found a key frame\n", stream_id, pts);
				found = true;
			} else if (false) {
				//printf ("ASFFrameReader::Seek (%d, %llu): Checking for audio frame..\n", stream_id, pts);
				asf_stream_properties* asp = parser->GetStream (StreamId ());
				found = asp->is_audio ();
				//printf ("ASFFrameReader::Seek (%d, %llu): Checking for audio frame: %s\n", stream_id, pts, found ? "true" : "false");
			}
			if (found)
				break;
		}
		counter++;
	}
	
	if (!found) {
		//printf ("ASFFrameReader::Seek (%d, %lld): Didn't find the requested pts.\n", stream_id, pts);
		return false;
	}
	
	//printf ("ASFFrameReader::Seek (%d, %llu): Counted to %d.\n", stream_id, pts, counter);
		
	current_packet_index = 0;
	RemoveAll ();
	
	while (counter-- > 0) {
		if (!Advance (stream_id)) {
			return false;
		}
		//printf ("ASFFrameReader::Seek (%d, %lld): Counting: %i, pts: %llu, size: %i.\n", stream_id, pts, counter, Pts (), Size ());
	}
	
	// Don't return any frames before the pts we seeked to.
	// This might happen if we found the requested pts in stream #1, but stream #2 
	// has frames with pts below this one.
	first_pts = pts; 
	
	//printf ("ASFFrameReader::Seek (%d, %llu): Found the requested pts, we're now at: %llu.\n", stream_id, pts, Pts ());
	
	return true;
}

bool
ASFFrameReader::Advance ()
{
	return Advance (0);
}

bool
ASFFrameReader::Advance (int stream_id)
{
start:
	ASF_LOG ("ASFFrameReader::Advance ().\n");
	// Clear the current list of payloads.
	
	// Most streams has at least once a media object spanning two payloads.
	// so we allocate space for two (+ NULL at the end).
	if (payloads == NULL) {
		if (!ResizeList (2)) {
			parser->AddError ("Data corruption in packets.");
			return false;
		}
	} else {
		// Free all old payloads, they belong to the previous frame.
		for (int i = 0; payloads[i]; i++) {
			delete payloads[i];
			payloads[i] = NULL;
		}
	}
	
	if (first == NULL) {
		if (!ReadMore ())
			return false;
		if (first == NULL)
			return false;
	}
	
	int payload_count = 0;
	uint32_t media_object_number = 0;
	ASFFrameReaderData* current = NULL;
	
	size = 0;
	
	ASF_LOG ("ASFFrameReader::Advance (): frame data: size = %lld, key = %s, pts = %llu, stream# = %d, media_object_number = %u.\n", 
		 size, IsKeyFrame () ? "true" : "false", Pts (), StreamId (), media_object_number);
	//asf_single_payload_dump (payloads [0]);
	current = first;
	
	while (true) {
		// First loop through payloads until we find a payload with the requested stream number.
		// Then continue looping through payloads until we find a payload with the same stream number.
		// if the media number is different, no more payloads in the current frame,
		// otherwise add the payload to the current frame's payloads and continue looping.
		while (current == NULL) {
			// We went past the end, read another packet to get more data.
			current = last; // go back to the last element.
			if (!ReadMore ()) {
				goto end_frame; // No more data, we've reached the end
			} else {
				if (current == NULL) {
					// There was no elements before reading more, our next element is the first one
					current = first;
				} else {
					current = current->next;
				}
			}
		}
		
		ASF_LOG ("ASFFrameReader::Advance (): checking payload, stream: %d, media object number %d, size: %d\n", current->payload->stream_id, current->payload->media_object_number, current->payload->payload_data_length);
		
		asf_single_payload* payload = current->payload;
		
		if (false && payload->presentation_time < first_pts) {
			ASFFrameReaderData* tmp = current;
			current = current->next;
			Remove (tmp);
		} else if (stream_id == 0 || payload->stream_id == stream_id) {
			if (payload_count > 0 && payload->media_object_number != media_object_number) {
				// We've found the end of the current frame's payloads
				ASF_LOG ("ASFFrameReader::Advance (): reached media object number %i (while reading %i).\n", payload->media_object_number, media_object_number);
				goto end_frame;
			}
			
			if (stream_id == 0)
				stream_id = payload->stream_id;
			
			media_object_number = payload->media_object_number;
			
			// add the payload to the current frame's payloads
			payload_count++;
			size += payload->payload_data_length;
			if (payload_count > payloads_size) {
				if (!ResizeList (payload_count + 3)) {
					return false;
				}
			}
			payloads [payload_count - 1] = payload;
			current->payload = NULL;
			
			// Remove it from the queue
			ASFFrameReaderData* tmp = current;
			current = current->next;
			Remove (tmp);
		} else {
			current = current->next;
		}
		
		ASF_LOG ("ASFFrameReader::Advance (): current is %p.\n", current);
	}
	
end_frame:
/*
	printf ("ASFFrameReader::Advance (): frame data: size = %.4lld, key = %s, pts = %.5llu, stream# = %i, media_object_number = %.3u, script_command_stream_index = %u (advanced).", 
		size, IsKeyFrame () ? "true " : "false", Pts (), StreamNumber (), media_object_number, script_command_stream_index);

	dump_int_data (payloads [0]->payload_data, payloads [0]->payload_data_length, 4);
	printf ("\n");
*/
//	printf ("ASFFrameReader::Advance (): frame data: size = %.4lld, key = %s, pts = %.5llu, stream# = %i, media_object_number = %.3u, script_command_stream_index = %u (advanced).\n", 
//		size, IsKeyFrame () ? "true " : "false", Pts (), StreamId (), media_object_number, script_command_stream_index);


	// Check if the current frame is a script command, in which case we must call the callback set in 
	// the parser (and read another frame).
	if (StreamId () == script_command_stream_index && script_command_stream_index > 0) {
		printf ("reading script command\n");
		ReadScriptCommand ();
		goto start;
	}
	
	return true;
}

void
ASFFrameReader::ReadScriptCommand ()
{
	uint64_t pts;
	char *text;
	char *type;
	gunichar2 *data;
	gunichar2 *uni_type = NULL;
	gunichar2 *uni_text = NULL;
	int text_length = 0;
	int type_length = 0;
	ASF_LOG ("ASFFrameReader::ReadScriptCommand (), size = %llu.\n", size);

	if (parser->embedded_script_command == NULL) {
		ASF_LOG ("ASFFrameReader::ReadScriptCommand (): no callback set.\n");
		return;
	}

	data = (gunichar2*) g_malloc (Size ());
	
	if (!Write (data)) {
		ASF_LOG ("ASFFrameReader::ReadScriptCommand (): couldn't read the data.\n");
		return;
	}
	
	uni_type = data;
	pts = Pts ();
	
	// the data is two arrays of WCHARs (type and text), null terminated.
	// loop through the data, counting characters and null characters
	// there should be at least two null characters.
	int null_count = 0;
	
	for (uint32_t i = 0; i < (size / sizeof (gunichar2)); i++) {
		if (uni_text == NULL) {
			type_length++;
		} else {
			text_length++;
		}
		if (*(data + i) == 0) {
			null_count++;
			if (uni_text == NULL) {
				uni_text = data + i + 1;
			} else {
				break; // Found at least two nulls
			}
		}
	}
	
	if (null_count >= 2) {
		text = wchar_to_utf8 (uni_text, text_length);
		type = wchar_to_utf8 (uni_type, type_length);
		
		ASF_LOG ("ASFFrameReader::ReadScriptCommand (): sending script command to %p, type: '%s', text: '%s', pts: '%llu'.\n", parser->embedded_script_command, type, text, pts);
		parser->embedded_script_command (parser->embedded_script_command_state, type, text, pts);
		
		g_free (text);
		g_free (type);
	} else {
		ASF_LOG ("ASFFrameReader::ReadScriptCommand (): didn't find 2 null characters in the data.\n");
	}
	
	g_free (data);

}

bool
ASFFrameReader::ReadMore ()
{
	ASF_LOG ("ASFFrameReader::ReadMore ().\n");
	
	ASFPacket* packet = new ASFPacket ();
	
	if ((uint32_t) current_packet_index >= parser->GetPacketCount ()) {
		eof = true;
		return false;
	}
	
	if (!parser->ReadPacket (packet, current_packet_index)) {
		ASF_LOG ("ASFFrameReader::ReadMore (): could not read more packets.\n");
		delete packet;
		return false;
	}
	
	if (CanSeek ())
		current_packet_index++;
	
	asf_single_payload** payloads = packet->payloads->steal_payloads ();
	int i = -1;
	while (payloads [++i] != NULL) {
		// Append the payload at the end of the queue of payloads.
		ASFFrameReaderData* node = new ASFFrameReaderData (payloads [i]);
			
		if (first == NULL) {
			first = node;
			last = node;
		} else {
			node->prev = last;
			last->next = node;
			last = node;
		}
	}
	g_free (payloads);
	
	ASF_LOG ("ASFFrameReader::ReadMore (): read %d payloads.\n", i);
	
	delete packet;
	return true;
}

bool
ASFFrameReader::Write (void *dest)
{
	if (payloads == NULL)
		return false;
	
	for (int i = 0; payloads[i]; i++) {
		memcpy (dest, payloads[i]->payload_data, payloads[i]->payload_data_length);
		dest = ((char *) dest) + payloads[i]->payload_data_length;
	}
	
	return true;
}

void 
ASFFrameReader::RemoveAll ()
{
	ASFFrameReaderData* current = first, *next = NULL;
	while (current != NULL) {
		next = current->next;
		delete current;
		current = next;
	}
	first = NULL;
	last = NULL;
}

void
ASFFrameReader::Remove (ASFFrameReaderData* data)
{
	if (data->prev != NULL)
		data->prev->next = data->next;
	
	if (data->next != NULL)
		data->next->prev = data->prev;
	
	if (data == first)
		first = data->next;
	
	if (data == last)
		last = data->prev;
	
	delete data;
}
