
// MFC_PlayerDlg.h : header file
//

#pragma once
#include "afxwin.h"

#include "FFPlayLib.h"

// CMFC_PlayerDlg dialog
class CMFC_PlayerDlg : public CDialogEx
{
// Construction
public:
	CMFC_PlayerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MFC_PLAYER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;
	
	FFP_EVENTS   m_FFP_events;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	CStatic m_Panel_YUV;
	CStatic m_Panel_RGB;
public:
	CButton m_Button_Play;
	CButton m_Button_Stop;
	void StartPlaying();
	void StopPlaying();
	void UpdateRGB(int w, int h, int bpp, BYTE* pBuffer);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnClose();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonPlay();
	virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
};
