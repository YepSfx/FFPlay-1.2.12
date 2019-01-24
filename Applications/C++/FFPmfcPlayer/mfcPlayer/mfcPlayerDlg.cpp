
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
}

static void OnEventInfo(void *sender, int infoCode, char *Message)
{
	CString msg(Message);
	OutputDebugString((LPCWSTR)msg);
}

static void OnEventAudio(void *sender, unsigned char *AudBuffer, int BufferLengthInByte)
{

}

static void OnEventVideo(void *sender, void *YuvData)
{
}

static void OnEventResize(void *sender, int width, int height)
{
}

static void OnEventStatus(void *sender, int status)
{
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
	DDX_Control(pDX, IDC_BUTTON_PLAY, mButtonPlay);
	DDX_Control(pDX, IDC_BUTTON_STOP, mButtonStop);
	DDX_Control(pDX, IDC_BUTTON_PAUSERESUME, mButtonPauseResume);
}

BEGIN_MESSAGE_MAP(CmfcPlayerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_PLAY, &CmfcPlayerDlg::OnBnClickedButtonPlay)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CmfcPlayerDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_PAUSERESUME, &CmfcPlayerDlg::OnBnClickedButtonPauseresume)
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

		if (SetupYuvPlayer(this, (unsigned int)mPanelYUV.m_hWnd, OnEventInfo, OnEventExit, OnEventAudio, OnEventResize, OnEventStatus) != 0)
		{
			ClearPlayer();
			MessageBox(_T("Setup Player Failed."));
			return;
		}


		if (StartPlayer() != 0)
		{
			ClearPlayer();
			MessageBox(_T("Open stream Failed."));
			return;
		}
	}
}

void CmfcPlayerDlg::OnBnClickedButtonStop()
{
	// TODO: Add your control notification handler code here
	StopPlayer();
}


void CmfcPlayerDlg::OnBnClickedButtonPauseresume()
{
	// TODO: Add your control notification handler code here
	PauseResumePlayer();
}
