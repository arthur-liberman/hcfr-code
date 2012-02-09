/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#include "../../stdafx.h"
#include "coords.h"
#include <vector>

using namespace std;

//CCoordinates
CCoordinates::CCoordinates(double _x1, double _x2, double _v1, double _v2)
{
	p_x1 = _x1; p_x2 = _x2;
	p_v1 = _v1; p_v2 = _v2;
	if (p_x1!=p_x2) capacity = (p_v2-p_v1)/(p_x2-p_x1);
	 else capacity = 0;
}

CCoordinates::CCoordinates(CCoordinates* src_coords)
{
    CCoordinates(src_coords->x1(), src_coords->x2(), src_coords->v1(), src_coords->v2());
}

int CCoordinates::GetExponent(double value)
{
	int res = 0;
	if (value!=0)
	{
		if (fabs(value)>=1)
		{
			res = (int)(log(fabs(value))/log(10));
		} else
		{
			res = (-1)*(int)(fabs(log(fabs(value)))/log(10)+1);
		};
	};
	return res;
}

double CCoordinates::CountGrid(int NMax, double mx1, double mx2)
{
	if (NMax==0) return 0.1f;
	double Delta,p, GridItem, Grid; 
	p=fabs(mx1-mx2);
	p=p/NMax;
	Delta=p;
//	if (Delta<=0.1) return (float)0.1;
	int Mult;
	GridItem=0.1f; Mult=1; Grid=0.1f;							   // Grid Items: 1,2,5,10,20,50...
	while (Delta>GridItem)
	{
		switch ((int)(10*Grid))
		{
			case 1 :{ Grid=0.2f; GridItem=Grid*Mult;}; break; //Grid = 0.1
			case 2 :{ Grid=0.5f; GridItem=Grid*Mult;};	break;
			case 5 :{ Grid=1; GridItem=Grid*Mult;};	break;
			case 10 :{ Grid=2; GridItem=Grid*Mult;}; break;
			case 20 :{ Grid=5; GridItem=Grid*Mult;};	break;
			case 50 :{ Grid=0.1f;Mult=Mult*10; GridItem=Grid*Mult;};	break; //Grid = 5
		};
	};
	return GridItem;
}

void CCoordinates::DivideInterval(int NMax, int* maxexp, double* Delta, int* num, double** scrpoints, double** wpoints)
{
	if (fabs(p_v1)<fabs(p_v2))
	{
		*maxexp = GetExponent(p_v2);
	} else
	{
		*maxexp = GetExponent(p_v1);
	};
	if (p_v1 == p_v2)
	{
		*Delta = 0;
		*num = NMax;
		*scrpoints = new double[*num];
		*wpoints = new double[*num];
		for (int i=0; i<*num; i++)
		{
			(*wpoints)[i] = p_v1;
			(*scrpoints)[i] = WtoX(p_v1);
		};
		return;		
	};
	double m_x1=p_v1*exp(-*maxexp*log(10));
	double m_x2=p_v2*exp(-*maxexp*log(10));
	while(fabs(m_x1-m_x2)<0.2f && m_x1<100 && m_x2<100)
	{
		*maxexp-=1;
		m_x1=p_v1*exp(-*maxexp*log(10));
		m_x2=p_v2*exp(-*maxexp*log(10));
	};
	*Delta = CountGrid(NMax, m_x1, m_x2);
	GetPointsForDelta(*Delta, num, scrpoints, wpoints, *maxexp);
}

void CCoordinates::GetPointsForDelta(double Delta, int* num, double** scrpoints, double** wpoints, int maxexp)
{
	double mv1, mv2;
	mv1 = p_v1*exp(-maxexp*log(10));
	mv2 = p_v2*exp(-maxexp*log(10));
	if (Delta>fabs(mv1-mv2))
	{
		*num = 0;
		*scrpoints = NULL;
		*wpoints = NULL;
		return;
	};
	//find the nearest point to left edge
	int pointnum = -1;
	double ras;
	int k = (int)(mv1/Delta);
	for (int i=0; i<5; i++)
	{
		double v = Delta*(k-2+i);
		if (IsPointInRange(v, mv1, mv2)) 
		{
			double r = fabs(v-mv1);
			if (pointnum == -1)
			{
				ras = r;
				pointnum = i;
			} else
			{
				if (r<ras)
				{
					ras = r;
					pointnum = i;
				};
			};
		};
	};
	if (pointnum == -1)
	{
		*num = 0;
		*scrpoints = NULL;
		*wpoints = NULL;
		return;
	};
	//enumerate all points
	vector<double> sp;
	vector<double> wp;
	double CurrentValue = Delta*(k-2+pointnum);
	double CurrentX;
	while(IsPointInRange(CurrentValue, mv1, mv2))
	{
		double v = CurrentValue*exp(maxexp*log(10));
		wp.push_back(v);
		CurrentX = WtoX(v);
		sp.push_back(CurrentX);
		//count next point
		if (p_v1<p_v2)
		{
			CurrentValue+=Delta;
		} else
		{
			CurrentValue-=Delta;
		};
	};
	//fill scrpoints and wpoints with results
	int asize = sp.size();
	if (asize == 0)
	{
		*num = 0;
		*scrpoints = NULL;
		*wpoints = NULL;
		return;
	};
	*num = asize;
	*scrpoints = new double[asize];
	*wpoints = new double[asize];
	for (i=0; i<asize; i++)
	{
		(*scrpoints)[i] = sp.at(i);
		(*wpoints)[i] = wp.at(i);
	};
}
