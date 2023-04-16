/*
 * map.c
 *
 *  Created on: 2021. nov. 11.
 *      Author: epagris
 */

#include "property_map.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "embfmt/embformat.h"

// structure for a single
typedef struct {
	char pKey[PM_MAX_PROPERTY_NAME_LENGTH + 1]; // name of the field
	PM_Type type; // type of referenced field
	void *pField; // pointer to field
	size_t count; // thread field as array with count length
} PM_Record;

// ---------------------------------------------------

static PM_Record spRecs[PM_PROPERTY_MAP_DEPTH]; // records storing references to fields
static size_t sFillLevel = 0;

// add new property to map
bool pm_add(const char *pKey, PM_Type type, const void *pField, size_t count) {
	if (sFillLevel >= PM_PROPERTY_MAP_DEPTH) {
		return false; // cannot add more properties to map
	}

	// get record
	PM_Record *pRec = &spRecs[sFillLevel];

	// fill in fields
	strncpy(pRec->pKey, pKey, PM_MAX_PROPERTY_NAME_LENGTH);
	pRec->type = type;
	pRec->pField = pField;
	pRec->count = count;

	// increase fill level
	sFillLevel++;

	return true;
}

#define PM_JSON_FIELD_BUF_LEN (1023)
static char * spJSONFieldBuf;

static void field_to_string(char *pStr, size_t maxLen, void *pField, PM_Type type, size_t offset) {
	if (maxLen < 8) { // some unusably little buffer size
		return;
	}

	int64_t i64;
	uint64_t u64;
	double dbl;

	switch (type) {
	// string and chars
	case PMT_STRING:
		pStr[0] = '"';
		strncpy(pStr + 1, pField, maxLen - 2);
		pStr[strlen(pStr) + 1] = '"';
		pStr[strlen(pStr) + 2] = '\0';
		break;
	case PMT_CHAR:
		pStr[2] = pStr[0] = '"';
		pStr[1] = *(((char*) pField) + offset);
		pStr[3] = '\0';
		break;

		// signed integers
	case PMT_INT8:
		i64 = *(((int8_t*) pField) + offset);
		break;
	case PMT_INT16:
		i64 = *(((int16_t*) pField) + offset);
		break;
	case PMT_INT32:
		i64 = *(((int32_t*) pField) + offset);
		break;
	case PMT_INT64:
		i64 = *(((int64_t*) pField) + offset);
		break;

		// unsigned integers
	case PMT_UINT8:
		u64 = *(((uint8_t*) pField) + offset);
		break;
	case PMT_UINT16:
		u64 = *(((uint16_t*) pField) + offset);
		break;
	case PMT_UINT32:
		u64 = *(((uint32_t*) pField) + offset);
		break;
	case PMT_UINT64:
		u64 = *(((uint64_t*) pField) + offset);
		break;

		// floating points
	case PMT_FLOAT:
		dbl = *(((float*) pField) + offset);
		break;
	case PMT_DOUBLE:
		dbl = *(((double*) pField) + offset);
		break;

		// boolean
	case PMT_BOOL:
		strncpy(pStr, (*(((bool*) pField) + offset) ? "true" : "false"), maxLen);
		break;

	default:
		printf("Unknown type!\n");
		break;
	}

	// number to string processing
	if (type >= PMT_INT8 && type <= PMT_INT64) {
		embfmt(pStr, maxLen, "%lli", i64);
	} else if (type >= PMT_UINT8 && type <= PMT_UINT64) {
		embfmt(pStr, maxLen, "%llu", u64);
	} else if (type >= PMT_FLOAT && type <= PMT_DOUBLE) {
		embfmt(pStr, maxLen, "%.5lf", dbl);
	}

}

#define CONCAT_AND_TRACK(src,dst,full,fill) strncpy(dst+fill,src,full-fill), fill += strlen(src)

void pm_output_json(char *pDestBuf, size_t destBufLen) {
	size_t destFillLevel = 0;

	if (destBufLen == 0) {
		return;
	}

	pDestBuf[0] = '\0';

	CONCAT_AND_TRACK("{", pDestBuf, destBufLen, destFillLevel);

	size_t i;
	for (i = 0; i < sFillLevel; i++) {
		const PM_Record *pRec = &spRecs[i];

		// print key
		CONCAT_AND_TRACK("\"", pDestBuf, destBufLen, destFillLevel);
		CONCAT_AND_TRACK(pRec->pKey, pDestBuf, destBufLen, destFillLevel);
		CONCAT_AND_TRACK("\"", pDestBuf, destBufLen, destFillLevel);
		CONCAT_AND_TRACK(": ", pDestBuf, destBufLen, destFillLevel);

		// print value

		if (pRec->count > 1) {
			CONCAT_AND_TRACK("[", pDestBuf, destBufLen, destFillLevel);
		}

		size_t k;
		for (k = 0; k < pRec->count; k++) {

			field_to_string(spJSONFieldBuf, PM_JSON_FIELD_BUF_LEN, pRec->pField, pRec->type, k);
			CONCAT_AND_TRACK(spJSONFieldBuf, pDestBuf, destBufLen, destFillLevel);

			if ((k + 1) < pRec->count) {
				CONCAT_AND_TRACK(",", pDestBuf, destBufLen, destFillLevel);
			}
		}

		if (pRec->count > 1) {
			CONCAT_AND_TRACK("]", pDestBuf, destBufLen, destFillLevel);
		}

		if ((i + 1) < sFillLevel) {
			CONCAT_AND_TRACK(",", pDestBuf, destBufLen, destFillLevel);
		}
	}

	CONCAT_AND_TRACK("}", pDestBuf, destBufLen, destFillLevel);
}

void pm_init() {
	spJSONFieldBuf = malloc(PM_JSON_FIELD_BUF_LEN);
}

