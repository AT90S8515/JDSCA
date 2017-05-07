#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    srinkwindow();
    setFixedSize(width(),height());

    //RTL freq timeout
    RTLFreqtimeout = new QTimer(this);
    connect(RTLFreqtimeout,SIGNAL(timeout()),this,SLOT(RTL_freq_timeout()));

    //set background of spinbox transperant
    QPalette pal = ui->doubleSpinBox_rtl_freq->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    ui->doubleSpinBox_rtl_freq->setPalette(pal);

    //create audio device and connect its output to a slot in this class to dipatch the data to the right place
    jsound = new TJCSound(this);
    jsound->iParameters.nChannels=2;
    jsound->oParameters.nChannels=0;
    jsound->sampleRate=SAMPLE_RATE_IN;
    jsound->audioformat=RTAUDIO_FLOAT64;
    jsound->options.streamName="JDSCA";
    jsound->bufferFrames=BUFFER_FRAMES;

    //create oqpsk demodulator
    oqpskdemod = new OqpskDemodulator(this);

    //create datadeformatter decoder
    datadeformatter = new DSCADataDeFormatter(this); //Create datadeformatter

    //create opus audio out decoder
    opusAudioOut = new OpusAudioOut(this);

    //create settings dialog.
    settingsdialog = new SettingsDialog(this);

    //create rtl
    sdr = new SDR(this);

    //connect the datadeformatter as the sink for the oqpsk demod output
    //should this be in the audio thread or the gui thread?
    //currently its not safe be could be made safe
connect(oqpskdemod,SIGNAL(demodulatedSoftBits(QByteArray)),datadeformatter,SLOT(processDemodulatedSoftBits(QByteArray)));
//connect(oqpskdemod,SIGNAL(demodulatedSoftBits(QByteArray)),datadeformatter,SLOT(processDemodulatedSoftBits(QByteArray)),Qt::DirectConnection);

//qDebug()<<"gui QThread"<<QThread::currentThreadId();

    //console setup
    ui->console->setEnabled(true);
    ui->console->setLocalEchoEnabled(true);

    //statusbar setup
    berlabel = new QLabel();
    freqlabel = new QLabel();
    ebnolabel = new QLabel();
    modelabel = new QLabel();
    bitratelabel = new QLabel();
    rtl_freq_label = new QLabel();
    //rtl_freq_spin = new QDoubleSpinBox();
    ui->statusBar->addPermanentWidget(new QLabel());
    ui->statusBar->addPermanentWidget(modelabel);
    ui->statusBar->addPermanentWidget(rtl_freq_label);
    //ui->statusBar->addPermanentWidget(rtl_freq_spin);
    ui->statusBar->addPermanentWidget(bitratelabel);
    ui->statusBar->addPermanentWidget(freqlabel);
    ui->statusBar->addPermanentWidget(berlabel);
    ui->statusBar->addPermanentWidget(ebnolabel);

    /*rtl_freq_spin->setFrame(false);
    rtl_freq_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    rtl_freq_spin->setAlignment(Qt::AlignHCenter);
    rtl_freq_spin->setSuffix(" MHz");
    QPalette pal = rtl_freq_spin->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    rtl_freq_spin->setPalette(pal);*/

    //led setup
    ui->ledvolume->setLED(QIcon::Off);
    ui->ledsignal->setLED(QIcon::Off);
    ui->ledopus->setLED(QIcon::Off);

    //misc connections
    connect(ui->action_About,    SIGNAL(triggered()),                                   this, SLOT(AboutSlot()));


    //for queued connection back
    qRegisterMetaType<QVector<cpx_type> >("QVector<cpx_type>");
    qRegisterMetaType<QVector<double> >("QVector<double>");
qRegisterMetaType<DSCADataDeFormatter::Mode>("DSCADataDeFormatter::Mode");



    //oqpsk connections
    connect(oqpskdemod, SIGNAL(Plottables(double,double,double)),              this,SLOT(PlottablesSlot(double,double,double)));
    connect(oqpskdemod, SIGNAL(SignalStatus(bool)),                            this,SLOT(SignalStatusSlot(bool)));
    connect(oqpskdemod, SIGNAL(WarningTextSignal(QString)),                    this,SLOT(WarningTextSlot(QString)));
    connect(oqpskdemod, SIGNAL(EbNoMeasurmentSignal(double)),                  this,SLOT(EbNoSlot(double)));
    connect(oqpskdemod, SIGNAL(PeakVolume(double)),                            this, SLOT(PeakVolumeSlot(double)));
    connect(oqpskdemod, SIGNAL(OrgOverlapedBuffer(QVector<double>)),           ui->spectrumdisplay,SLOT(setFFTData(QVector<double>)));
    connect(oqpskdemod, SIGNAL(Plottables(double,double,double)),              ui->spectrumdisplay,SLOT(setPlottables(double,double,double)));
    connect(oqpskdemod, SIGNAL(SampleRateChanged(double)),                     ui->spectrumdisplay,SLOT(setSampleRate(double)));
    connect(oqpskdemod, SIGNAL(ScatterPoints(QVector<cpx_type>)),              ui->scatterplot,SLOT(setData(QVector<cpx_type>)));
    connect(ui->spectrumdisplay,   SIGNAL(CenterFreqChanged(double)),                     oqpskdemod,SLOT(CenterFreqChangedSlot(double)));
    connect(oqpskdemod, SIGNAL(BitRateChanged(double)),                        datadeformatter,SLOT(setBitRate(double)));
    connect(oqpskdemod, SIGNAL(BitRateChanged(double)),                        this,SLOT(bitRateChangedSlot(double)));
    connect(oqpskdemod, SIGNAL(SignalStatus(bool)),                            datadeformatter,SLOT(SignalStatusSlot(bool)));

//direct connection for sound thread
connect(jsound,SIGNAL(SoundEvent(double*,double*,int)),oqpskdemod,SLOT(demodFromStereoSoundEvent(double*,double*,int)),Qt::DirectConnection);

//direct connection for RTL device thread
connect(sdr,SIGNAL(audio_signal_out(const double*,qint64,int)),oqpskdemod,SLOT(demodData(const double*,qint64,int)),Qt::DirectConnection);

    //datadeformatter connections
    connect(datadeformatter,SIGNAL(DataCarrierDetect(bool)),this,SLOT(DataCarrierDetectStatusSlot(bool)));
    connect(datadeformatter,SIGNAL(DataCarrierDetect(bool)),oqpskdemod,SLOT(DCDstatSlot(bool)));
connect(datadeformatter,SIGNAL(signalDSCAOpusPacket(QByteArray)),opusAudioOut,SLOT(decode(QByteArray)));
    connect(datadeformatter,SIGNAL(signalModeChanged(DSCADataDeFormatter::Mode)),this,SLOT(on_modeChanged(DSCADataDeFormatter::Mode)));
    connect(datadeformatter,SIGNAL(signalBER(double)),this,SLOT(on_BERstatus(double)));
connect(datadeformatter,SIGNAL(signalDSCARDSPacket(QByteArray)),this,SLOT(DSCARDSPacketSlot(QByteArray)));

    //load settings
    QSettings settings("Jontisoft", "JDSCA");
    ui->doubleSpinBox_rtl_freq->setValue(settings.value("doubleSpinBox_rtl_freq",91.1).toDouble());
    ui->comboBoxdisplay->setCurrentIndex(settings.value("comboBoxdisplay",0).toInt());
    ui->horizontalSlider_bw->setValue(settings.value("bandwidth",10000).toDouble());
    double tmpfreq=settings.value("freq_center",1000).toDouble(); 
    double tmpbandwidth=settings.value("bandwidth",10000).toDouble();
    if(!settings.value("frame_spectrum",1).toBool())QTimer::singleShot(0,this,SLOT(on_actionSpectrum_triggered()));
    if(!settings.value("groupBox_scatterplot",1).toBool())QTimer::singleShot(0,this,SLOT(on_actionConstellation_triggered()));

    //set audio demodulator settings and start
    on_comboBoxdisplay_currentIndexChanged(ui->comboBoxdisplay->currentText());
    on_horizontalSlider_bw_sliderMoved(ui->horizontalSlider_bw->value());

    //oqpsk setup
    oqpskdemodulatorsettings.Fs=SAMPLE_RATE_IN;
    oqpskdemodulatorsettings.freq_center=tmpfreq;
    oqpskdemodulatorsettings.lockingbw=tmpbandwidth;
    oqpskdemod->setSettings(oqpskdemodulatorsettings);

    //audio device input sutup
    jsound->iParameters.nChannels=2;
    jsound->oParameters.nChannels=0;
    jsound->sampleRate=SAMPLE_RATE_IN;
    jsound->audioformat=RTAUDIO_FLOAT64;
    jsound->options.streamName="JDSCA";
    jsound->bufferFrames=BUFFER_FRAMES;
    jsound->wantedInDeviceName=settingsdialog->WantedInSoundDevice;

    QTimer *rtlstatusupdate= new QTimer;
    connect(rtlstatusupdate,SIGNAL(timeout()),this,SLOT(RTL_status_timeout()));
    rtlstatusupdate->start(250);

    //start input device
    if(settingsdialog->WantedInSoundDevice=="RTL SDR")
    {
        oqpskdemodulatorsettings.Fs=100000;
        oqpskdemod->setSettings(oqpskdemodulatorsettings);
        sdr->OpenRtl(settingsdialog->rtl_device_index);
        sdr->StartRtl();
        sdr->SetAGC(settingsdialog->rtl_agc);
        sdr->SetAFC(settingsdialog->rtl_afc);
        sdr->SetGain(settingsdialog->rtl_gain);
        sdr->SetFrequency(ui->doubleSpinBox_rtl_freq->value()*1000000);
        sdr->SetSubCarrierFrequencyOffset(settingsdialog->rtl_subcarrier_freq_offset);
        sdr->SetFilterSelection(settingsdialog->rtl_filter_selection);
        ui->doubleSpinBox_rtl_freq->clearFocus();
        ui->spectrumdisplay->setFrequencyOffset(settingsdialog->rtl_subcarrier_freq_offset);
    } else jsound->Active(true);




    //Opusaudioout connections
    connect(opusAudioOut,SIGNAL(OpusDCDchanged(bool)),this,SLOT(OpusSignalStatusSlot(bool)));

    //set pop and accept settings
    settingsdialog->populatesettings();
    acceptsettings();

    //RDS display timeout
    RDStimeout = new QTimer(this);
    connect(RDStimeout,SIGNAL(timeout()),this,SLOT(RDStimeoutSlot()));



    RTLFreqtimeout->start(1000);

}

void MainWindow::closeEvent(QCloseEvent *event)
{

    //save settings
    QSettings settings("Jontisoft", "JDSCA");
    settings.setValue("bandwidth", oqpskdemod->getBandwidth());
    settings.setValue("comboBoxdisplay", ui->comboBoxdisplay->currentIndex());
    settings.setValue("freq_center", oqpskdemod->getCurrentFreq());
    settings.setValue("frame_spectrum", ui->frame_spectrum->isVisible());
    settings.setValue("groupBox_scatterplot", ui->groupBox_scatterplot->isVisible());
    settings.setValue("doubleSpinBox_rtl_freq", ui->doubleSpinBox_rtl_freq->value());

    event->accept();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::OpusSignalStatusSlot(bool signal)
{
    if(signal)ui->ledopus->setLED(QIcon::On);
     else ui->ledopus->setLED(QIcon::Off);
}

void MainWindow::SignalStatusSlot(bool signal)
{
    if(signal)ui->ledsignal->setLED(QIcon::On);
     else ui->ledsignal->setLED(QIcon::Off);
}

void MainWindow::DataCarrierDetectStatusSlot(bool dcd)
{
    if(dcd)ui->leddata->setLED(QIcon::On);
     else {ui->leddata->setLED(QIcon::Off);ui->label_rds->clear();}
}

//guess EbNo using MER
void MainWindow::EbNoSlot(double EbNo)
{
    ebnolabel->setText(((QString)" MER: %1dB ").arg(qRound(round(EbNo)),2, 10, QChar(' ')));
}

void MainWindow::WarningTextSlot(QString warning)
{
    ui->statusBar->showMessage("Warning: "+warning,5000);
}

void MainWindow::PeakVolumeSlot(double Volume)
{
    if(Volume>0.9&&jsound->IsActive())ui->ledvolume->setLED(QIcon::On,QIcon::Active);
     else if(Volume<0.1)ui->ledvolume->setLED(QIcon::Off);
      else ui->ledvolume->setLED(QIcon::On);
}

void MainWindow::bitRateChangedSlot(double bitrate)
{
    bitratelabel->setText(((QString)" %1 bps  ").arg(bitrate));
}

void MainWindow::PlottablesSlot(double freq_est,double freq_center,double bandwidth)
{
    Q_UNUSED(freq_center);
    Q_UNUSED(bandwidth);
    if(sdr->active)freq_est+=settingsdialog->rtl_subcarrier_freq_offset;
    QString str=(((QString)"%1Hz  ").arg(freq_est,0, 'f', 2)).rightJustified(11,' ');
    freqlabel->setText("  Freq: "+str);
}

void MainWindow::on_BERstatus(double ber)
{
    QString str=(((QString)"%1% ").arg(ber*100.0,0, 'f', 0)).rightJustified(6,' ');
    berlabel->setText("  PE: "+str);
    //berlabel->setText(((QString)" BER: %1% ").arg((int)round(ber*100),2, 10, QChar('0')));
}

void MainWindow::AboutSlot()
{
    QMessageBox::about(this,"JDSCA",""
                                     "<H1>A DSCA demodulator and decoder</H1>"
                                     "<H3>v1.1.2</H3>"
                                     "<p>This is a program to listen to digital SCA signals.</p>"
                                     "<p><a href=\"http://jontio.zapto.org/hda1/jdsca.html\">http://jontio.zapto.org/hda1/jdsca.html</a>.</p>"
                                     "<p>Jonti 2017</p>" );


}

void MainWindow::on_modeChanged(DSCADataDeFormatter::Mode mode)
{


    switch(mode)
    {
    case DSCADataDeFormatter::mode0:
        modelabel->setText("  mode0  ");
        break;
    case DSCADataDeFormatter::mode1:
        modelabel->setText("  mode1  ");
        break;
    case DSCADataDeFormatter::mode2:
        modelabel->setText("  mode2  ");
        break;
    case DSCADataDeFormatter::mode3:
        modelabel->setText("  mode3  ");
        break;
    case DSCADataDeFormatter::mode4:
        modelabel->setText("  mode4  ");
        break;
    default:
        modelabel->setText("  none   ");
    break;
    }

}

void MainWindow::on_comboBoxdisplay_currentIndexChanged(const QString &arg1)
{
    ui->scatterplot->graph(0)->clearData();
    ui->scatterplot->replot();
    ui->scatterplot->setDisksize(3);
    if(arg1=="Constellation")
    {
        oqpskdemod->setScatterPointType(OqpskDemodulator::SPT_constellation);
    }
    if(arg1=="Constellation 8")
    {
        oqpskdemod->setScatterPointType(OqpskDemodulator::SPT_8constellation);
    }
    if(arg1=="Symbol phase")
    {
        oqpskdemod->setScatterPointType(OqpskDemodulator::SPT_phaseoffsetest);
        ui->scatterplot->setDisksize(6);
    }
    if(arg1=="None")
    {
        oqpskdemod->setScatterPointType(OqpskDemodulator::SPT_None);
    }
}

void MainWindow::on_action_Settings_triggered()
{
    settingsdialog->populatesettings();
    if(settingsdialog->exec()==QDialog::Accepted)acceptsettings();
}

void MainWindow::acceptsettings()
{

    ui->statusBar->clearMessage();
    opusAudioOut->set_soundcard_name(settingsdialog->WantedOutSoundDevice); 
    if(jsound->wantedInDeviceName!=settingsdialog->WantedInSoundDevice)
    {
        jsound->Active(false);
        sdr->StopAndCloseRtl();

        jsound->wantedInDeviceName=settingsdialog->WantedInSoundDevice;

        //rtl device or soundcard
        if(settingsdialog->WantedInSoundDevice=="RTL SDR")
        {
            oqpskdemodulatorsettings.Fs=100000;
            oqpskdemodulatorsettings.freq_center=oqpskdemod->getCurrentFreq();
            oqpskdemodulatorsettings.lockingbw=oqpskdemod->getBandwidth();
            oqpskdemod->setSettings(oqpskdemodulatorsettings);
            sdr->OpenRtl(settingsdialog->rtl_device_index);
            sdr->StartRtl();
        }
         else
         {
            oqpskdemodulatorsettings.Fs=SAMPLE_RATE_IN;
            oqpskdemodulatorsettings.freq_center=oqpskdemod->getCurrentFreq();
            oqpskdemodulatorsettings.lockingbw=oqpskdemod->getBandwidth();
            oqpskdemod->setSettings(oqpskdemodulatorsettings);
            jsound->Active(true);
         }


    }

    oqpskdemod->setCMA(settingsdialog->use_cma);
    datadeformatter->enableHardFECDecoderType(settingsdialog->use_hard_decoding);
    oqpskdemod->setAFC(settingsdialog->use_tracking);

    sdr->SetAGC(settingsdialog->rtl_agc);
    sdr->SetAFC(settingsdialog->rtl_afc);
    sdr->SetGain(settingsdialog->rtl_gain);
    sdr->SetFrequency(ui->doubleSpinBox_rtl_freq->value()*1000000);
    sdr->SetSubCarrierFrequencyOffset(settingsdialog->rtl_subcarrier_freq_offset);
    sdr->SetFilterSelection(settingsdialog->rtl_filter_selection);
    ui->doubleSpinBox_rtl_freq->clearFocus();

    if(sdr->active)ui->spectrumdisplay->setFrequencyOffset(settingsdialog->rtl_subcarrier_freq_offset);
     else ui->spectrumdisplay->setFrequencyOffset(0);


}

void MainWindow::ERRorslot(QString &error)
{
    ui->console->appendHtml("<font color=\"red\">"+error+"</font>");
}

void MainWindow::DSCARDSPacketSlot(const QByteArray &ba)
{
    if(ba.size()<2)return;
    QString PS=ba.mid(1,ba[0]);
    QString RT=ba.mid(ba[0]+2);
    ui->label_rds->setText("<h2>"+PS+"</h2>"+RT);
    RDStimeout->start(10000);
}

void MainWindow::RDStimeoutSlot()
{
    ui->label_rds->clear();
    RDStimeout->stop();
}

void MainWindow::on_actionSpectrum_triggered()
{
    if(ui->frame_spectrum->isVisible())
    {
        ui->frame_spectrum->setVisible(false);
        ui->horizontalSlider_bw->setVisible(false);
        if(!ui->comboBoxdisplay->isVisible())ui->widget_bar->setVisible(false);
        //come on qt this is nasty
#ifdef __SLOW_SRINK_WINDOW__
        QTimer::singleShot(500,this,SLOT(srinkwindow()));
#else
        QTimer::singleShot(50,this,SLOT(srinkwindow()));
#endif
        QApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
    }
     else
     {
        ui->frame_spectrum->setVisible(true);
        ui->widget_bar->setVisible(true);
        ui->horizontalSlider_bw->setVisible(true);
     }


}

void MainWindow::on_actionConstellation_triggered()
{
    if(ui->groupBox_scatterplot->isVisible())
    {
        ui->groupBox_scatterplot->setVisible(false);
        ui->comboBoxdisplay->setVisible(false);
        if(!ui->frame_spectrum->isVisible())ui->widget_bar->setVisible(false);
        //come on qt this is nasty
#ifdef __SLOW_SRINK_WINDOW__
        QTimer::singleShot(500,this,SLOT(srinkwindow()));
#else
        QTimer::singleShot(50,this,SLOT(srinkwindow()));
#endif
        QApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
    }
     else
     {
        ui->groupBox_scatterplot->setVisible(true);
        ui->comboBoxdisplay->setVisible(true);
        ui->widget_bar->setVisible(true);
     }
}

void MainWindow::srinkwindow()
{
    resize(0,0);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_horizontalSlider_bw_sliderMoved(int position)
{
    oqpskdemod->setBandwidth(position);
}

void MainWindow::on_doubleSpinBox_rtl_freq_valueChanged(double arg1)
{
    sdr->SetFrequency(arg1*1000000);
    RTLFreqtimeout->start(10000);
}

void MainWindow::RTL_freq_timeout()
{
    RTLFreqtimeout->stop();
    ui->doubleSpinBox_rtl_freq->clearFocus();
    ui->centralWidget->setFocus();
}

void MainWindow::RTL_status_timeout()
{
    if(!sdr->active)
    {
        if(!rtl_freq_label->isHidden())
        {
            rtl_freq_label->hide();
            ui->doubleSpinBox_rtl_freq->hide();
        }
    }
     else
     {
        if(rtl_freq_label->isHidden())
        {
            rtl_freq_label->show();
            ui->doubleSpinBox_rtl_freq->show();
        }
     }
    QString str=(((QString)"  %1MHz  ").arg(((double)sdr->GetFrequency())/1000000.0,0, 'f', 2)).rightJustified(11,' ');
    rtl_freq_label->setText(str);
}
