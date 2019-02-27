
// mfcPlayerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mfcPlayer.h"
#include "mfcPlayerDlg.h"
#include "afxdialogex.h"

#include "FFP_decoder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <string>

#define		DEF_RGB

#define WM_USER_UPDATERGB		WM_USER+0x07

static BYTE RGBBuffer[4096 * 4096 * 4] = { 0, };

typedef struct{
	int w;
	int h;
	int bpp;
	BYTE *pBuffer;
}RGBDATA;

std::string utf8_encode(const std::wstring &wstr)
{
	if (wstr.empty()) 
		return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	
	return strTo;
}

static void OnEventExit(void *sender, int exitCode)
{
	OutputDebugString(_T("Exit Event\n"));
}

static void OnEventInfo(void *sender, int infoCode, char *Message)
{
	CString msg = CString(Message);
	CString code;
	code.Format(_T(" infoCode: %d"), infoCode);
	msg = CString(_T(">Eventinfo: ")) + msg + code + CString(_T("\n"));
	OutputDebugString(msg);
}

static void OnEventAudio(void *sender, unsigned char *AudBuffer, int BufferLengthInByte)
{
	CString msg;
	msg.Format(_T(">Audio event 0x%x, %d\n"), AudBuffer, BufferLengthInByte);
//	OutputDebugString(msg);
}

static void OnEventVideo(void *sender, void *pYuvData)
{
	int w, h;
	HWND handle = ((CWnd*)sender)->m_hWnd;
	RGBDATA *pRGB = new(RGBDATA);
	
	ConvertYUV420pToRGB24(pYuvData, RGBBuffer, &w, &h);

	pRGB->w = w;
	pRGB->h = h;
	pRGB->bpp = 3;
	pRGB->pBuffer = RGBBuffer;

	::PostMessage(handle, WM_USER_UPDATERGB, (WPARAM)pRGB, 0);
}

static void OnEventResize(void *sender, int width, int height)
{

}

static void OnEventStatus(void *sender, int status)
{
	CString msg;
	msg.Format(_T(">>Play Status Info: %d\n"), (int)status);
	OutputDebugString(msg);
}

CmfcPlayerDlg::CmfcPlayerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CmfcPlayerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CmfcPlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PANEL_YUV, mPanelYUV);
	DDX_Control(pDX, IDC_PANEL_RGB, mPanelRGB);
	DDX_Control(pDX, IDC_BUTTON_PLAY, mButtonPlay);
	DDX_Control(pDX, IDC_BUTTON_STOP, mButtonStop);
	DDX_Control(pDX, IDC_BUTTON_PAUSERESUME, mButtonPauseResume);
	DDX_Control(pDX, IDC_BUTTON_CLIPLAY, mButtonCLI);
}

BEGIN_MESSAGE_MAP(CmfcPlayerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_PLAY, &CmfcPlayerDlg::OnBnClickedButtonPlay)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CmfcPlayerDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_PAUSERESUME, &CmfcPlayerDlg::OnBnClickedButtonPauseresume)
	ON_MESSAGE(WM_USER_UPDATERGB, &CmfcPlayerDlg::OnRGBUpdate)
	ON_BN_CLICKED(IDC_BUTTON_CLIPLAY, &CmfcPlayerDlg::OnBnClickedButtonCliplay)
END_MESSAGE_MAP()


// CmfcPlayerDlg message handlers

BOOL CmfcPlayerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	//ShowWindow(SW_MINIMIZE);

	// TODO: Add extra initialization here
#ifdef DEF_RGB
	mPanelYUV.EnableWindow(FALSE);
	mPanelYUV.ShowWindow(SW_HIDE);
	mPanelRGB.EnableWindow(TRUE);
	mPanelRGB.ShowWindow(SW_SHOW);
#else
	mPanelYUV.EnableWindow(TRUE);
	mPanelYUV.ShowWindow(SW_SHOW);
	mPanelRGB.EnableWindow(FALSE);
	mPanelRGB.ShowWindow(SW_HIDE);
#endif
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CmfcPlayerDlg::OnPaint()
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
HCURSOR CmfcPlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CmfcPlayerDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialogEx::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO:  Add your specialized creation code here

	int rtn = InitilizeDLL();

	if (rtn)
	{
		MessageBox(_T("Fail to import DLL"));
	}

	return 0;
}


void CmfcPlayerDlg::OnClose()
{
	// TODO: Add your message handler code here and/or call default
	FinalizeDLL();

	CDialogEx::OnClose();
}

void CmfcPlayerDlg::OnBnClickedButtonPlay()
{
	// TODO: Add your control notification handler code here
	CFileDialog OpenFile(TRUE);
	if (OpenFile.DoModal() == IDOK)
	{
		const char *pFileName;
		mFileName = OpenFile.GetPathName();

		std::wstring wsFileName(mFileName);
		std::string  utf8filename = utf8_encode(wsFileName);
		
		pFileName = utf8filename.c_str();

		SetPlayerFileName(pFileName);

#ifdef DEF_RGB
		if (SetupRgbPlayer(this, (unsigned int)mPanelYUV.m_hWnd, OnEventInfo, OnEventExit, OnEventAudio, OnEventVideo, OnEventResize, OnEventStatus) != 0)
		{
			ClearPlayer();
			MessageBox(_T("Setup Player Failed."));
			return;
		}
		mPanelYUV.EnableWindow(FALSE);
		mPanelYUV.ShowWindow(SW_HIDE);
		mPanelRGB.EnableWindow(TRUE);
		mPanelRGB.ShowWindow(SW_SHOW);
#else
		if (SetupYuvPlayer(this, (unsigned int)mPanelYUV.m_hWnd, OnEventInfo, OnEventExit, OnEventAudio, OnEventResize, OnEventStatus) != 0)
		{
			ClearPlayer();
			MessageBox(_T("Setup Player Failed."));
			return;
		}
		mPanelYUV.EnableWindow(TRUE);
		mPanelYUV.ShowWindow(SW_SHOW);
		mPanelRGB.EnableWindow(FALSE);
		mPanelRGB.ShowWindow(SW_HIDE);
#endif

#if 1
		if (StartThreadPlayer() != 0)
		{
			ClearPlayer();
			MessageBox(_T("Open stream Failed."));
			return;
		}
#else
		if (OpenPlayerStream() != 0)
		{
			ClearPlayer();
			MessageBox(_T("Cannot open stream!"));
			return;
		}
		
		StartPlayer();
#endif
	}
}

void CmfcPlayerDlg::OnBnClickedButtonStop()
{
	// TODO: Add your control notification handler code here
	StopThreadPlayer();
}

void CmfcPlayerDlg::OnBnClickedButtonPauseresume()
{
	// TODO: Add your control notification handler code here
	PauseResumePlayer();
}

void CmfcPlayerDlg::UpdateScreen(int w, int h, int bpp, BYTE* pBuffer)
{
	HDC dc = ::GetDC(mPanelRGB.m_hWnd);

	if (dc == NULL)
	{
		OutputDebugString(_T("Cannot acquire DC"));
		return;
	}

	BITMAPINFOHEADER bmpInfo;
	bmpInfo.biBitCount = 24;
	bmpInfo.biClrImportant = 0;
	bmpInfo.biClrUsed = 0;
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

	::ReleaseDC(mPanelRGB.m_hWnd, dc);
}

LONG CmfcPlayerDlg::OnRGBUpdate(unsigned int wparam, LONG lparam)
{
	RGBDATA *pRGB = (RGBDATA*)(wparam);

	UpdateScreen(pRGB->w, pRGB->h, pRGB->bpp, pRGB->pBuffer);

	delete pRGB;

	return 0;
}

void CmfcPlayerDlg::OnBnClickedButtonCliplay()
{
	// TODO: Add your control notification handler code here
	CFileDialog OpenFile(TRUE);
	if (OpenFile.DoModal() == IDOK)
	{
		const char *pFileName;
		mFileName = OpenFile.GetPathName();

		std::wstring wsFileName(mFileName);
		std::string  utf8filename = utf8_encode(wsFileName);

		pFileName = utf8filename.c_str();

		SetPlayerFileName(pFileName);

		if (SetupCLIPlayer(this, OnEventInfo, OnEventExit, OnEventAudio, OnEventResize, OnEventStatus) != 0)
		{
			ClearPlayer();
			MessageBox(_T("Setup Player Failed."));
			return;
		}

		if (OpenPlayerStream() != 0)
		{
			ClearPlayer();
			MessageBox(_T("Cannot open stream!"));
			return;
		}

		StartPlayer();

	}

}
