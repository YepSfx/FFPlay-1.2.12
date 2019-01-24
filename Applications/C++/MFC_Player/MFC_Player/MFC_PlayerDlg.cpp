
// MFC_PlayerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MFC_Player.h"
#include "MFC_PlayerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//#define		DEF_RGB

// CMFC_PlayerDlg dialog

#include <string>

// CMFC_FFPlayDlg dialog
#define WM_USER_UPDATERGB		WM_USER+0x07

static BYTE RGBBuffer[4096 * 4096 * 4] = { 0, };

typedef struct{
	int w;
	int h;
	int bpp;
	BYTE *pBuffer;
}RGBDATA;

void __cdecl EventExit(void *sender, int exitCode)
{
	OutputDebugString(_T("Exit Event\n"));
}

void __cdecl Eventinfo(void *sender, int infoCode, char *pMsg)
{
	CString msg = CString(pMsg);
	CString code;
	code.Format(_T(" infoCode: %d"), infoCode);
	msg = CString(_T(">Eventinfo: ")) + msg + code +CString(_T("\n"));
	OutputDebugString(msg);
}

void __cdecl EventAudio(void *sender, BYTE *pBuffer, int BufferLenInByte)
{

}

void __cdecl EventPlayStatus(void *sender, FFP_PLAY_STATUS status)
{
	CString msg;
	msg.Format(_T(">>Play Status Info: %d\n"), (int)status);
	OutputDebugString(msg);
}

void __cdecl EventVideo(void *sender, FFP_YUV420P_DATA *pYUVData)
{
	HWND handle = ((CWnd*)sender)->m_hWnd;
	RGBDATA *pRGB = new(RGBDATA);
	multimedia_yuv420p_to_rgb24(pYUVData, RGBBuffer);

	pRGB->w   = pYUVData->w;
	pRGB->h   = pYUVData->h;
	pRGB->bpp = 3;
	pRGB->pBuffer = RGBBuffer;

	::PostMessage(handle, WM_USER_UPDATERGB, (WPARAM)pRGB, 0);
}

void __cdecl EventResize(void *sender, int width, int height)
{
	OutputDebugString(_T("Resize Event\n"));
}

std::string utf8_encode(const std::wstring &wstr)
{
	if (wstr.empty())
		return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);

	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

	return strTo;
}

static void ThreadStreaming(void *pParam)
{
	multimedia_stream_start();
}

CMFC_PlayerDlg::CMFC_PlayerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMFC_PlayerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFC_PlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PANEL_YUV, m_Panel_YUV);
	DDX_Control(pDX, IDC_PANEL_RGB, m_Panel_RGB);
	DDX_Control(pDX, IDC_BUTTON_PLAY, m_Button_Play);
	DDX_Control(pDX, IDC_BUTTON_STOP, m_Button_Stop);
}

BEGIN_MESSAGE_MAP(CMFC_PlayerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CMFC_PlayerDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_PLAY, &CMFC_PlayerDlg::OnBnClickedButtonPlay)
END_MESSAGE_MAP()


// CMFC_PlayerDlg message handlers

BOOL CMFC_PlayerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMFC_PlayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMFC_PlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMFC_PlayerDlg::StartPlaying()
{
	_beginthread(ThreadStreaming, 0, NULL);
}

void CMFC_PlayerDlg::StopPlaying()
{
	OutputDebugString(_T("Stop playing...\n"));
	multimedia_stream_stop();
	OutputDebugString(_T("Playing stopped.\n"));
}

BOOL CMFC_PlayerDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class
	if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_F4 || pMsg->wParam == VK_ESCAPE)
		return TRUE;

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CMFC_PlayerDlg::OnClose()
{
	// TODO: Add your message handler code here and/or call default
	if (multimedia_event_loop_alive() == 1)
	{
		StopPlaying();
	}

	CDialogEx::OnClose();
}

void CMFC_PlayerDlg::OnBnClickedButtonStop()
{
	// TODO: Add your control notification handler code here
	StopPlaying();
}

void CMFC_PlayerDlg::OnBnClickedButtonPlay()
{
	// TODO: Add your control notification handler code here
	CFileDialog  fileDialog(TRUE, _T("avi"), NULL, OFN_HIDEREADONLY,
		_T("AVI file (*.avi)|*.avi|ASF file (*.asf)|*.asf|All file (*.*)|*.*|"), NULL);

	if (fileDialog.DoModal() == IDOK)
	{
		m_FFP_events.sender = this;
		m_FFP_events.event_info = Eventinfo;
		m_FFP_events.screenID = (unsigned long)m_Panel_YUV.m_hWnd;
		m_FFP_events.ui_type = FFP_GUI;
		m_FFP_events.event_exit = EventExit;
		m_FFP_events.event_audio = EventAudio;
		m_FFP_events.event_video_resize = EventResize;
		m_FFP_events.event_play_status = EventPlayStatus;
		m_FFP_events.playstatus = FFP_STOP;
#ifdef DEF_RGB		
		m_FFP_events.event_video = EventVideo;
		m_Panel_YUV.EnableWindow(FALSE);
		m_Panel_YUV.ShowWindow(SW_HIDE);
		m_Panel_RGB.EnableWindow(TRUE);
		m_Panel_RGB.ShowWindow(SW_SHOW);
#else
		m_FFP_events.event_video = NULL;
		m_Panel_YUV.EnableWindow(TRUE);
		m_Panel_YUV.ShowWindow(SW_SHOW);
		m_Panel_RGB.EnableWindow(FALSE);
		m_Panel_RGB.ShowWindow(SW_HIDE);
#endif
		CString fileName = fileDialog.GetPathName();

		std::wstring wsFileName(fileName);
		std::string utf8filename = utf8_encode(wsFileName);

		const char* pFileName = utf8filename.c_str();

		multimedia_set_filename(pFileName);

		if (multimedia_init_device(&m_FFP_events) != 0)
		{
			multimedia_exit();
			MessageBox(_T("Fail to Init!"));
			return;
		}

		if (multimedia_stream_open() == FFP_FALSE)
		{
			multimedia_exit();
			MessageBox(_T("Fail to open file!"));
			return;
		}

		StartPlaying();
	}
}

void CMFC_PlayerDlg::UpdateRGB(int w, int h, int bpp, BYTE* pBuffer)
{
	HDC dc = ::GetDC(m_Panel_RGB.m_hWnd);

	if (dc == NULL)
	{
		OutputDebugString(_T("Cannot acquire DC"));
		return;
	}

	BITMAPINFOHEADER bmpInfo;
	bmpInfo.biBitCount = 24;
	bmpInfo.biClrImportant = 0;
	bmpInfo.biClrUsed  = 0;
	bmpInfo.biCompression = BI_RGB;
	bmpInfo.biHeight = -h;
	bmpInfo.biPlanes = 1;
	bmpInfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpInfo.biSizeImage = w * h * 3;
	bmpInfo.biWidth = w;
	bmpInfo.biXPelsPerMeter = 0;
	bmpInfo.biYPelsPerMeter = 0;


	if (0 == SetDIBitsToDevice(dc, 0, 0, w, h, 0, 0, 0, h, (const void*)pBuffer, (const BITMAPINFO*)&bmpInfo, DIB_RGB_COLORS))
	{
		OutputDebugString(_T("Cannot display RGB!\n"));
	}
	
	::ReleaseDC(m_Panel_RGB.m_hWnd, dc);
}

BOOL CMFC_PlayerDlg::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	// TODO: Add your specialized code here and/or call the base class
	if (message == WM_USER_UPDATERGB)
	{
		RGBDATA *pRGB = (RGBDATA*)(wParam);
		UpdateRGB(pRGB->w, pRGB->h, pRGB->bpp, pRGB->pBuffer);
		delete pRGB;

		return TRUE;
	};

	return CDialogEx::OnWndMsg(message, wParam, lParam, pResult);
}
