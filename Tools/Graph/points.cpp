/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#include "../../stdafx.h"
#include "points.h"

CPointsCollection::CPointsCollection(BOOL b_sort_x/* = TRUE*/, BOOL b_keep_same_x/* = FALSE*/)
{
	bSortX = b_sort_x;
	bKeepSameX = b_keep_same_x;
	max_x = min_x = max_y = min_y = 0;
}

CPointsCollection::CPointsCollection(CPointsCollection* pcol)
{
	max_x = pcol->max_x;
	min_x = pcol->min_x;
	max_y = pcol->max_y;
	min_y = pcol->min_y;
	Flags = pcol->Flags;
	bSortX = pcol->bSortX;
	bKeepSameX = pcol->bKeepSameX;
	SSinglePoint ssp;
	for (int i=0; i<pcol->GetSize(); i++)
	{
		if (pcol->GetPoint(i, &ssp) != -1)
		{
			points.push_back(ssp);
		};
	};
}

int CPointsCollection::AddPoint(SSinglePoint* csp, BOOL bReScan, int* res)
{
	return AddPoint(csp->x, csp->y, bReScan, res);
}

int CPointsCollection::AddPoint(double _x, double _y, BOOL bReScan, int* res)
{
	int index;
	//adjust max and min
	if (GetSize() == 0)
	{
		max_x = min_x = _x;
		max_y = min_y = _y;
	} else
	{
		if (max_x<_x) max_x = _x;
		if (min_x>_x) min_x = _x;
		if (max_y<_y) max_y = _y;
		if (min_y>_y) min_y = _y;
	};
	SSinglePoint ssp(_x, _y);
	SSinglePoint spoint;
	long array_size = GetSize();
	if (!bSortX)
	{//if there is no sort - just add the point to the end of array
		points.push_back(ssp);
		index = array_size;
	} else
	{
		//find the place for new element
		if (array_size == 0)
		{
			points.push_back(ssp);
			index = array_size;
		} else
		{
			//check lower bound
			GetPoint(0, &spoint);
			if (_x<spoint.x)
			{
				index = 0;
				points.insert(points.begin() + index, ssp);
			} else
			{
				//check upper bound
				GetPoint(array_size-1, &spoint);
				if (_x>spoint.x)
				{
					points.push_back(ssp);
					index = array_size;
				} else
				{
					index = 0;
					BOOL bFit = FALSE;
					for (int i=0; i<array_size; i++)
					{
						GetPoint(i, &spoint);
						if (spoint.x == _x)
						{
							index = i;
							bFit = TRUE;
							break;
						};
						if (_x<spoint.x)
						{
							index = i;
							break;
						};
					};
					if (bFit)
					{
						if (bKeepSameX)
						{
							points.insert(points.begin() + index, ssp);
						} else
						{
							RemovePoint(index, FALSE);//do not rescan right away - cause we add one more point right after
							points.insert(points.begin() + index, ssp);
							//need to recalculate extremums
							if (bReScan) RescanPoints();
						};
					} else
					{
						//just insert point before index
						points.insert(points.begin() + index, ssp);
					};
				};
			};
		};
	};
	return index;
}

void CPointsCollection::RemovePoint(int index, BOOL bReScan)
{
	if (index>=0 && index<GetSize())
	{
		points.erase(points.begin() + index);
		if (bReScan) RescanPoints();	
	};
}

void CPointsCollection::RemovePointX(double x, BOOL bReScan)
{
	SSinglePoint ssp;
	BOOL bFit = FALSE;
	for (int i=0; i<GetSize(); i++)
	{
		if (GetPoint(i, &ssp) != -1)
		{
			if (ssp.x == x)
			{
				RemovePoint(i, FALSE);
				i-=1;
				bFit = TRUE;
			};
		};
	};
	if (bReScan) RescanPoints();	
}

void CPointsCollection::RemovePointY(double y, BOOL bReScan)
{
	SSinglePoint ssp;
	BOOL bFit = FALSE;
	for (int i=0; i<GetSize(); i++)
	{
		if (GetPoint(i, &ssp) != -1)
		{
			if (ssp.y == y)
			{
				RemovePoint(i, FALSE);
				i-=1;
				bFit = TRUE;
			};
		};
	};
	if (bReScan) RescanPoints();	
}

int CPointsCollection::GetPoint(int index, double* x, double *y)
{
	SSinglePoint ssp;
	if (GetPoint(index, &ssp)!=-1)
	{
		*x = ssp.x;
		*y = ssp.y;
		return index;
	};
	return -1;
}

int CPointsCollection::GetNearestPoint(double _x, double _y, SSinglePoint* result)
{
	return -1;
}

void CPointsCollection::RescanPoints()
{
	int size = GetSize();
	if (size == 0)
	{
		max_x = min_x = max_y = min_y = 0;
		return;
	};
	SSinglePoint ssp;
	GetPoint(0, &ssp);
	min_x = max_x = ssp.x;
	min_y = max_y = ssp.y;
	for (int i=1; i<size; i++)
	{
		GetPoint(i, &ssp);
		if (ssp.x<min_x) 
		{
			min_x = ssp.x;
		} else
		{
			if (ssp.x>max_x) 
			{
				max_x = ssp.x;
			};
		};
		if (ssp.y<min_y) 
		{
			min_y = ssp.y;
		} else
		{
			if (ssp.y>max_y) 
			{
				max_y = ssp.y;
			};
		};
	};
}

int CPointsCollection::EditPoint(int index, double x, double y, BOOL bRescan)
{
	if (index<0 || index>=GetSize()) return -1;
	int ret;
	SSinglePoint ssp(x, y);
	//if bSortX not specified - just replace given point
	RemovePoint(index, FALSE);
	if (!bSortX)
	{
		InsertPoint(index, ssp.x, ssp.y, bRescan);
		ret = index;
	} else
	{
		int res;
		ret = AddPoint(&ssp, bRescan, &res);
	};
	return ret;
}

int CPointsCollection::InsertPoint(int index, double x, double y, BOOL bRescan)
{
	if (index<0 || index>GetSize()) return -1;
	SSinglePoint ssp(x, y);
	int res, ret;
	if (index == GetSize())
	{ //this is just append point
		ret = AddPoint(&ssp, bRescan, &res);
	} else
	{
	if (!bSortX)
	{
		points.insert(points.begin() + index, ssp);
		if (bRescan) RescanPoints();
		ret = index;
	} else
	{
		ret = AddPoint(&ssp, bRescan, &res);
	};
	};
	return ret;
}

void CPointsCollection::RemoveAll()
{
	points.clear();
	max_x = min_x = max_y = min_y = 0;
}
