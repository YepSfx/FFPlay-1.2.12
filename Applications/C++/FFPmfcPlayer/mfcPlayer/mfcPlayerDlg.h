
// mfcPlayerDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CmfcPlayerDlg dialog
class CmfcPlayerDlg : public CDialogEx
{
// Construction
public:
	CmfcPlayerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MFCPLAYER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL	OnInitDialog();
	afx_msg void	OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg int		OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void	OnClose();
private:
	CStatic			mPanelYUV;
	CButton			mButtonPlay;
	CButton			mButtonStop;
	CButton			mButtonPauseResume;
	CString         mFileName;
public:
	afx_msg void	OnBnClickedButtonPlay();
	afx_msg void	OnBnClickedButtonStop();
	afx_msg void	OnBnClickedButtonPauseresume();
};
