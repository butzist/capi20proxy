/*
 *   capi20proxy (Provides a remote CAPI port over TCP/IP)
 *   Copyright (C) 2002  Adam Szalkowski <adam@szalkowski.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * $Log$
 */

/*
    Declarations of protocol structs
*/

enum DATA_TYPE
{
TYPE_REGISTER,
TYPE_RELEASE,
TYPE_PUT_MESSAGE,
TYPE_GET_MESSAGE,
TYPE_WAIT_FOR_SIGNAL,
TYPE_GET_MANUFACTURER,
TYPE_GET_VERSION,
TYPE_GET_SERIAL_NUMBER,
TYPE_GET_PROFILE,
TYPE_INSTALLED,
TYPE_WHO_AM_I
};

struct REQUEST_DATA_REGISTER
{
	DWORD MessageBufferSize;
	DWORD maxLogicalConnection;
	DWORD maxBDataBlocks;
	DWORD maxBDataLen;
};

struct REQUEST_DATA_RELEASE
{
	DWORD ApplID;
};

struct REQUEST_DATA_PUT_MESSAGE
{
	DWORD ApplID;
	SOCKADDR_IN me;
};

struct REQUEST_DATA_GET_MESSAGE
{
	DWORD ApplID;
	SOCKADDR_IN me;
};

struct REQUEST_DATA_WAIT_FOR_SIGNAL
{
	DWORD ApplID;
};

struct REQUEST_DATA_GET_MANUFACTURER
{
};

struct REQUEST_DATA_GET_VERSION
{
};

struct REQUEST_DATA_GET_SERIAL_NUMBER
{
};

struct REQUEST_DATA_GET_PROFILE
{
	DWORD CtrlNr;
};

struct REQUEST_DATA_INSTALLED
{
};

struct REQUEST_DATA_WHO_AM_I
{
};

struct ANSWER_DATA_REGISTER
{
	DWORD ret;
	DWORD ApplID;
};

struct ANSWER_DATA_RELEASE
{
	DWORD ret;
};

struct ANSWER_DATA_PUT_MESSAGE
{
	DWORD ret;
};

struct ANSWER_DATA_GET_MESSAGE
{
	DWORD ret;
};

struct ANSWER_DATA_WAIT_FOR_SIGNAL
{
	DWORD ret;
};

struct ANSWER_DATA_GET_MANUFACTURER
{
	DWORD ret;
	char szBuffer[64];
};

struct ANSWER_DATA_GET_VERSION
{
	DWORD ret;
	DWORD CAPIMajor;
	DWORD CAPIMinor;
	DWORD ManufacturerMajor;
	DWORD MAnufacturerMinor;
};

struct ANSWER_DATA_GET_SERIAL_NUMBER
{
	DWORD ret;
	char szBuffer[8];
};

struct ANSWER_DATA_GET_PROFILE
{
	DWORD ret;
	char szBuffer[64];
};

struct ANSWER_DATA_INSTALLED
{
	DWORD ret;
};

struct ANSWER_DATA_WHO_AM_I
{
	SOCKADDR_IN you;
};

struct REQUEST
{
	DATA_TYPE type;
	union REQUEST_DATA{
		REQUEST_DATA_REGISTER rd_register;
		REQUEST_DATA_RELEASE rd_release;
		REQUEST_DATA_PUT_MESSAGE rd_put_message;
		REQUEST_DATA_GET_MESSAGE rd_get_message;
		REQUEST_DATA_WAIT_FOR_SIGNAL rd_wait_for_signal;
		REQUEST_DATA_GET_MANUFACTURER rd_get_manufacturer;
		REQUEST_DATA_GET_VERSION rd_get_version;
		REQUEST_DATA_GET_SERIAL_NUMBER rd_get_serial_number;
		REQUEST_DATA_GET_PROFILE rd_get_profile;
		REQUEST_DATA_INSTALLED rd_installed;
		REQUEST_DATA_WHO_AM_I rd_who_am_i;
	} data;
};

struct ANSWER
{
	DATA_TYPE type;
	union ANSWER_DATA{
		ANSWER_DATA_REGISTER ad_register;
		ANSWER_DATA_RELEASE ad_release;
		ANSWER_DATA_PUT_MESSAGE ad_put_message;
		ANSWER_DATA_GET_MESSAGE ad_get_message;
		ANSWER_DATA_WAIT_FOR_SIGNAL ad_wait_for_signal;
		ANSWER_DATA_GET_MANUFACTURER ad_get_manufacturer;
		ANSWER_DATA_GET_VERSION ad_get_version;
		ANSWER_DATA_GET_SERIAL_NUMBER ad_get_serial_number;
		ANSWER_DATA_GET_PROFILE ad_get_profile;
		ANSWER_DATA_INSTALLED ad_installed;
		ANSWER_DATA_WHO_AM_I ad_who_am_i;
	} data;
};
