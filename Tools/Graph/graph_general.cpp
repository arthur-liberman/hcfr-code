/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#include "../../stdafx.h"
#include "graph_general.h"
#include "graphwnd.h"

CGraphWnd* CGraphBaseClass::get_main_graph_window()
{
    CWnd* wnd = dynamic_cast<CWnd*>(this);
    CGraphWnd* res_wnd = dynamic_cast<CGraphWnd*>(wnd);
    while(res_wnd == NULL)
    {
	wnd = wnd->GetOwner();
	res_wnd = dynamic_cast<CGraphWnd*>(wnd);
    };
    return res_wnd;
}

void CGraphBaseClass::EnumerateParentWindows(BaseClassWndCallbackFunc enum_func, void* user_data)
{
    CWnd* wnd = dynamic_cast<CWnd*>(this);
    do
    {
	CGraphBaseClass* res_wnd = dynamic_cast<CGraphBaseClass*>(wnd);
	if (res_wnd != NULL)
	{
	    if ((*enum_func)(res_wnd, user_data) != 0)
	    {
		break;
	    };
	};
	wnd = wnd->GetOwner();
    } while(wnd != NULL);

}

int AppendMenuForAllParents(CGraphBaseClass* base_class, void* param)
{
    CMenu* menu = reinterpret_cast<CMenu*>(param);
    base_class->AppendMenuItems(menu);
    return 0;
}

int OnRBMenuCommandForAllParents(CGraphBaseClass* base_class, void* param)
{
    UINT command_id = reinterpret_cast<UINT>(param);
    base_class->OnRBMenuCommand(command_id);
    return 0;
}

int AppendPropertyPageForAllParents(CGraphBaseClass* base_class, void* param)
{
    CPropertySheet* prop_sheet = reinterpret_cast<CPropertySheet*>(param);
    base_class->AppendPropertyPage(prop_sheet);
    return 0;
}

int ReleasePropertyPageForAllParents(CGraphBaseClass* base_class, void* param)
{
    UINT res_status = reinterpret_cast<UINT>(param);
    base_class->ReleasePropertyPage(res_status);
    return 0;
}

