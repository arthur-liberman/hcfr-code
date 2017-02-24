#pragma once

#include "../../libccast/ccmdns.h"

class CGoogleCastWrapper
{
public:
	CGoogleCastWrapper(void);
	~CGoogleCastWrapper(void);

	void RefreshList();
	int getCount() const { return m_count; }

	const ccast_id *operator[](int index) const { return m_ids ? m_ids[index] : 0; }
	const ccast_id *operator[](const char *name) const;
	const ccast_id *getCcastByIp(unsigned int ip) const;
	unsigned int getCcastIpAddress(int index) const { return m_ids ? getCcastIpAddress(m_ids[index]) : 0; }
	unsigned int getCcastIpAddress(const ccast_id *id) const;

private:
	void init();
	void cleanUp();

	ccast_id **m_ids;
	int m_count;
};

