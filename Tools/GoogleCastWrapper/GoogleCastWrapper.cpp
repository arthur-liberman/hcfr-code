#include "GoogleCastWrapper.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h> // For OutputDebugString.

CGoogleCastWrapper::CGoogleCastWrapper(void) : m_ids(NULL), m_count(0) 
{ }

CGoogleCastWrapper::~CGoogleCastWrapper(void)
{
	this->cleanUp();
}

void CGoogleCastWrapper::RefreshList()
{
	this->cleanUp();
	this->m_ids = get_ccids();
	ccast_id **id = this->m_ids;
	while (*id++)
		this->m_count++;

	char buf[1024];
	sprintf_s(buf, "Found %d Google Cast devices", this->m_count);
	OutputDebugStringA(buf);
	id = this->m_ids;
	while (*id)
	{
		sprintf_s(buf, "%s (%s) at: %s", (*id)->name, cctype2str((*id)->typ), (*id)->ip);
		OutputDebugStringA(buf);
		id++;
	}
}

const ccast_id *CGoogleCastWrapper::operator[](const char *name) const
{
	ccast_id *ret = NULL;

	for (int i = 0; i < this->m_count; i++)
	{
		if (!strcmp(this->m_ids[i]->name, name))
		{
			ret = this->m_ids[i];
			break;
		}
	}
	
	return ret;
}

const ccast_id *CGoogleCastWrapper::getCcastByIp(unsigned int ip) const
{
	ccast_id *ret = NULL;
	char ipStr[14];

	sprintf_s(ipStr,  "%d.%d.%d.%d", (ip >> 24) & 0xFF,  (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
	OutputDebugStringA(ipStr);
	for (int i = 0; i < this->m_count; i++)
	{
		OutputDebugStringA(this->m_ids[i]->ip);
		if (!strcmp(this->m_ids[i]->ip, ipStr))
		{
			ret = this->m_ids[i];
			break;
		}
	}
	
	return ret;
}

unsigned int CGoogleCastWrapper::getCcastIpAddress(const ccast_id *id) const
{
	char *delims = ".", *next, *token, *temp;
	unsigned char shl = 24;
	unsigned int ret = 0;

	temp = _strdup(id->ip);
	OutputDebugStringA(temp);
	token = strtok_s(temp, delims, &next);
	while (token)
	{
		OutputDebugStringA(token);
		ret |= (atoi(token) & 0xFF) << shl;
		shl -= 8;
		token = strtok_s(NULL, delims, &next);
	}
	free(temp);

	return ret;
}

void CGoogleCastWrapper::cleanUp()
{
	if (this->m_ids)
	{
		free_ccids(this->m_ids);
		this->m_ids = NULL;
		this->m_count = 0;
	}
}
