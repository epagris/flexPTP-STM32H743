/*
 * map
 *
 *  Created on: 2021. nov. 11.
 *      Author: epagris
 */

#ifndef PROPERTY_MAP_H_
#define PROPERTY_MAP_H_

#define PM_MAX_PROPERTY_NAME_LENGTH (31)
#define PM_PROPERTY_MAP_DEPTH (64)

#include <stdbool.h>
#include <stddef.h>

// enum for property types
typedef enum {
	PMT_STRING, PMT_CHAR, PMT_INT8, PMT_INT16, PMT_INT32, PMT_INT64, PMT_UINT8, PMT_UINT16, PMT_UINT32, PMT_UINT64, PMT_FLOAT, PMT_DOUBLE, PMT_BOOL
} PM_Type;

bool pm_add(const char *pKey, PM_Type type, const void *pField, size_t count); // add property to map
void pm_output_json(char *pDestBuf, size_t destBufLen); // output map in json format

#endif /* PROPERTY_MAP_H_ */
