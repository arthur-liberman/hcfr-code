/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#ifndef _POINTS_H_
#define _POINTS_H_

#include <vector>

using namespace std;

typedef struct SSinglePoint
{
	SSinglePoint(){x = 0; y = 0;};
	SSinglePoint(double _x, double _y){x = _x; y = _y;};

	double x;
	double y;
}SSinglePoint;

class CPointsCollection
{
	public:
		CPointsCollection(BOOL b_sort_x = TRUE, BOOL b_keep_same_x = FALSE);
		CPointsCollection(CPointsCollection* pcol);

		int AddPoint(SSinglePoint* csp, BOOL bReScan, int* res);
		int AddPoint(double _x, double _y, BOOL bReScan, int* res);

		void RemovePoint(int index, BOOL bReScan);
		void RemovePointX(double x, BOOL bReScan);
		void RemovePointY(double y, BOOL bReScan);
		void RemoveAll();

		int EditPoint(int index, double x, double y, BOOL bRescan);
		int InsertPoint(int index, double x, double y, BOOL bRescan);

		int GetPoint(int index, double* x, double *y);
		int GetPoint(int index, SSinglePoint* result)
		{
		    *result = points.at(index);
		    return index;
		};

		int GetNearestPoint(double _x, double _y, SSinglePoint* result);

		int GetSize(){return points.size();};
		void RescanPoints();

		vector<SSinglePoint> points;
		DWORD Flags;
		double max_x, max_y, min_x, min_y;
		BOOL bSortX, bKeepSameX;
};

#endif