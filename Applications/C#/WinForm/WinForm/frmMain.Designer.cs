namespace WinForm
{
    partial class frmMain
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.mButtonPlay = new System.Windows.Forms.Button();
            this.mButtonPause = new System.Windows.Forms.Button();
            this.openFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.mButtonStop = new System.Windows.Forms.Button();
            this.mPanelRGB = new System.Windows.Forms.Label();
            this.mPanelYUV = new System.Windows.Forms.Panel();
            this.SuspendLayout();
            // 
            // mButtonPlay
            // 
            this.mButtonPlay.Location = new System.Drawing.Point(12, 463);
            this.mButtonPlay.Name = "mButtonPlay";
            this.mButtonPlay.Size = new System.Drawing.Size(94, 23);
            this.mButtonPlay.TabIndex = 1;
            this.mButtonPlay.Text = "Play";
            this.mButtonPlay.UseVisualStyleBackColor = true;
            this.mButtonPlay.Click += new System.EventHandler(this.mButtonPlay_Click);
            // 
            // mButtonPause
            // 
            this.mButtonPause.Location = new System.Drawing.Point(224, 463);
            this.mButtonPause.Name = "mButtonPause";
            this.mButtonPause.Size = new System.Drawing.Size(94, 23);
            this.mButtonPause.TabIndex = 2;
            this.mButtonPause.Text = "Pause";
            this.mButtonPause.UseVisualStyleBackColor = true;
            this.mButtonPause.Click += new System.EventHandler(this.mButtonPause_Click);
            // 
            // openFileDialog
            // 
            this.openFileDialog.FileName = "openFileDialog1";
            // 
            // mButtonStop
            // 
            this.mButtonStop.Location = new System.Drawing.Point(112, 463);
            this.mButtonStop.Name = "mButtonStop";
            this.mButtonStop.Size = new System.Drawing.Size(94, 23);
            this.mButtonStop.TabIndex = 3;
            this.mButtonStop.Text = "Stop";
            this.mButtonStop.UseVisualStyleBackColor = true;
            this.mButtonStop.Click += new System.EventHandler(this.mButtonStop_Click);
            // 
            // mPanelRGB
            // 
            this.mPanelRGB.Location = new System.Drawing.Point(670, 26);
            this.mPanelRGB.Name = "mPanelRGB";
            this.mPanelRGB.Size = new System.Drawing.Size(340, 289);
            this.mPanelRGB.TabIndex = 6;
            this.mPanelRGB.Text = "RGB";
            // 
            // mPanelYUV
            // 
            this.mPanelYUV.Location = new System.Drawing.Point(24, 26);
            this.mPanelYUV.Name = "mPanelYUV";
            this.mPanelYUV.Size = new System.Drawing.Size(584, 394);
            this.mPanelYUV.TabIndex = 8;
            // 
            // frmMain
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1035, 498);
            this.Controls.Add(this.mPanelYUV);
            this.Controls.Add(this.mPanelRGB);
            this.Controls.Add(this.mButtonStop);
            this.Controls.Add(this.mButtonPause);
            this.Controls.Add(this.mButtonPlay);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.Name = "frmMain";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "FFPlay WinForm";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.frmMain_FormClosing);
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.frmMain_FormClosed);
            this.Load += new System.EventHandler(this.frmMain_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button mButtonPlay;
        private System.Windows.Forms.Button mButtonPause;
        private System.Windows.Forms.OpenFileDialog openFileDialog;
        private System.Windows.Forms.Button mButtonStop;
        private System.Windows.Forms.Label mPanelRGB;
        private System.Windows.Forms.Panel mPanelYUV;
    }
}

