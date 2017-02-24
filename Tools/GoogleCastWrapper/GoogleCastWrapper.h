#pragma once

#include "../../libccast/ccmdns.h"

class CGoogleCastWrapper
{
public:
	CGoogleCastWrapper(void);
	~CGoogleCastWrapper(void);

	void RefreshList();

	const ccast_id *operator[](int index) const { return m_ids[index]; }
	const ccast_id *operator[](const char *name) const;
	const ccast_id *getCcastByIp(unsigned int ip) const;
	unsigned int getCcastIpAddress(int index) const { return getCcastIpAddress(m_ids[index]); }
	unsigned int getCcastIpAddress(const ccast_id *id) const;

	int getCount() const { return m_count; }

private:
	void init();
	void cleanUp();

	ccast_id **m_ids;
	int m_count;
};

