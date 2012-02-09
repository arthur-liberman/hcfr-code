/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#ifndef _GRAPH_GENERAL_H_
#define _GRAPH_GENERAL_H_

//notification messages
#define CM_GRAPH_NOTIFICATION 50000

enum GRAPH_WM_COMMANDS
{
    //notification constants
    GRAPH_POINT_ADDED	    =	0x00000001,
    GRAPH_POINT_REMOVED,
    GRAPH_POINT_CHANGED,
//graph notify constants
    GRAPH_GRAPH_ADDED,
    GRAPH_GRAPH_REMOVED,
    GRAPH_GRAPH_CLEARED,
    GRAPH_PROP_CHANGED,
//graph window notifications
    GRAPH_GRAPHIX_PROP,
    GRAPH_GRAPHIX_AXISPROPS,
    GRAPH_GRAPHIX_UPDATE,
    GRAPH_PVIEW_ENABLED,
    GRAPH_PVIEW_DISABLED
};

enum GRAPH_PANEL_FLAGS
{
    GRAPH_AUTOSCALE	    =	0x00000001,
    GRAPH_SQUAREPOINTS	    =	0x00000002,
    GRAPH_SHOW_TOOLTIP	    =	0x00000004,
    GRAPH_DRAW_AXIS	    =	0x00000008,
    GRAPH_GRAPH_SCATTER	    =	0x00000010
};

//AXIS definition for notification commands
#define GRAPH_X_AXIS	TRUE
#define GRAPH_Y_AXIS	FALSE

//right button menu commands
enum GRAPH_RB_MENU_COMMANDS
{
    GRAPH_RBMC_FIRST	=   0x1000,
    GRAPH_RBMC_TOGGLE_POINT_MARKS,
    GRAPH_RBMC_TOGGLE_SCATTER_MODE,
    GRAPH_RBMC_TRACE_MOUSE,
    GRAPH_RBMC_SHOW_AXIS,
    GRAPH_RBMC_ZOOM_TOOL,
    GRAPH_RBMC_APPLY_ZOOM,
    GRAPH_RBMC_FIT_WIDTH,
    GRAPH_RBMC_FIT_HEIGHT,
    GRAPH_RBMC_FIT_PAGE,
    GRAPH_RBMC_VIEW_POINT_WINDOW,
    GRAPH_RBMC_VIEW_PROPERTIES,
    GRAPH_RBMC_LAST
};

enum GRAPH_WINDOW_UPDATE_VALUES
{
    GRAPH_WUV_GRAPH	=	0x00000001,
    GRAPH_WUV_PVIEW	=	0x00000002,
    GRAPH_WUV_RULERS	=	0x00000004,
    GRAPH_WUV_ALL	=	0xFFFFFFFF
};

enum GRAPH_PVIEW_OPERATIONS
{
    GRAPH_PO_SHOW	=	0x00000001,
    GRAPH_PO_HIDE	=	0x00000002,
    GRAPH_PO_DISABLE	=	0x00000004,
    GRAPH_PO_ENABLE	=	0x00000008
};

class CGraphWnd;

struct SGraphChange
{
    int graphnum;
    int index;
    int index_ex;
    BOOL bRedraw;
    CGraphWnd* main_wnd_ptr;
};

struct SGraphixProps
{
    double x1, x2, y1, y2;
    DWORD flags;
    BOOL bRedraw;
    CGraphWnd* main_wnd_ptr;
};

struct SAxisPropsChange
{
    BOOL bXAxis;
    BOOL bRedraw;
    CGraphWnd* main_wnd_ptr;
};

class CGraphBaseClass
{
    public:

	typedef int(*BaseClassWndCallbackFunc)(CGraphBaseClass* base_class, void* param);

    public:
	CGraphBaseClass(){};

	virtual CGraphWnd* get_main_graph_window();
	virtual void AppendMenuItems(CMenu* menu){};
	virtual void OnRBMenuCommand(UINT command_id){};
	virtual void AppendPropertyPage(CPropertySheet* prop_sheet){};
	virtual void ReleasePropertyPage(UINT dialog_status){};

	void EnumerateParentWindows(BaseClassWndCallbackFunc enum_func, void* user_data);
};

int AppendMenuForAllParents(CGraphBaseClass* base_class, void* param);
int OnRBMenuCommandForAllParents(CGraphBaseClass* base_class, void* param);

int AppendPropertyPageForAllParents(CGraphBaseClass* base_class, void* param);
int ReleasePropertyPageForAllParents(CGraphBaseClass* base_class, void* param);

#endif
    