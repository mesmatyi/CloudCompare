#include "Lanelet2MapDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

Lanelet2MapDialog::Lanelet2MapDialog( QWidget* parent )
	: QDialog( parent )
	, m_summaryEdit( new QLineEdit( this ) )
	, m_outputPathEdit( new QLineEdit( this ) )
	, m_originLatitudeSpinBox( new QDoubleSpinBox( this ) )
	, m_originLongitudeSpinBox( new QDoubleSpinBox( this ) )
	, m_originAltitudeSpinBox( new QDoubleSpinBox( this ) )
	, m_laneWidthSpinBox( new QDoubleSpinBox( this ) )
	, m_sampleCountSpinBox( new QSpinBox( this ) )
	, m_axisComboBox( new QComboBox( this ) )
{
	setWindowTitle( "Lanelet2 Map Demo" );
	setMinimumWidth( 560 );

	auto* mainLayout = new QVBoxLayout( this );
	auto* summaryLabel = new QLabel( "Selected cloud summary", this );
	mainLayout->addWidget( summaryLabel );

	m_summaryEdit->setReadOnly( true );
	mainLayout->addWidget( m_summaryEdit );

	auto* formLayout = new QFormLayout();
	mainLayout->addLayout( formLayout );

	auto* outputLayout = new QHBoxLayout();
	auto* outputWidget = new QWidget( this );
	auto* browseButton = new QPushButton( "Browse…", this );
	outputLayout->addWidget( m_outputPathEdit, 1 );
	outputLayout->addWidget( browseButton );
	outputWidget->setLayout( outputLayout );
	formLayout->addRow( "Output `.osm` file", outputWidget );

	m_originLatitudeSpinBox->setDecimals( 8 );
	m_originLatitudeSpinBox->setRange( -90.0, 90.0 );
	formLayout->addRow( "Origin latitude", m_originLatitudeSpinBox );

	m_originLongitudeSpinBox->setDecimals( 8 );
	m_originLongitudeSpinBox->setRange( -180.0, 180.0 );
	formLayout->addRow( "Origin longitude", m_originLongitudeSpinBox );

	m_originAltitudeSpinBox->setDecimals( 3 );
	m_originAltitudeSpinBox->setRange( -10000.0, 100000.0 );
	formLayout->addRow( "Origin altitude (m)", m_originAltitudeSpinBox );

	m_laneWidthSpinBox->setDecimals( 3 );
	m_laneWidthSpinBox->setRange( 0.1, 1000.0 );
	m_laneWidthSpinBox->setValue( 3.5 );
	formLayout->addRow( "Lane width (m)", m_laneWidthSpinBox );

	m_sampleCountSpinBox->setRange( 2, 200 );
	m_sampleCountSpinBox->setValue( 2 );
	formLayout->addRow( "Boundary sample count", m_sampleCountSpinBox );

	m_axisComboBox->addItem( "Auto (longest XY extent)" );
	m_axisComboBox->addItem( "X axis" );
	m_axisComboBox->addItem( "Y axis" );
	formLayout->addRow( "Lane direction", m_axisComboBox );

	auto* noteLabel = new QLabel(
		"Coordinates are exported from the cloud's global/original coordinates. "
		"Use the same LocalCartesian origin when loading the Lanelet2 map.",
		this );
	noteLabel->setWordWrap( true );
	mainLayout->addWidget( noteLabel );

	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
	mainLayout->addWidget( buttons );

	connect( browseButton, &QPushButton::clicked, this, &Lanelet2MapDialog::chooseOutputPath );
	connect( buttons, &QDialogButtonBox::accepted, this, &Lanelet2MapDialog::validateAndAccept );
	connect( buttons, &QDialogButtonBox::rejected, this, &Lanelet2MapDialog::reject );
}

void Lanelet2MapDialog::setCloudSummary( const QString& summary )
{
	m_summaryEdit->setText( summary );
}

QString Lanelet2MapDialog::outputPath() const
{
	return m_outputPathEdit->text().trimmed();
}

double Lanelet2MapDialog::originLatitude() const
{
	return m_originLatitudeSpinBox->value();
}

double Lanelet2MapDialog::originLongitude() const
{
	return m_originLongitudeSpinBox->value();
}

double Lanelet2MapDialog::originAltitude() const
{
	return m_originAltitudeSpinBox->value();
}

double Lanelet2MapDialog::laneWidth() const
{
	return m_laneWidthSpinBox->value();
}

int Lanelet2MapDialog::sampleCount() const
{
	return m_sampleCountSpinBox->value();
}

Lanelet2MapDialog::Axis Lanelet2MapDialog::axis() const
{
	return static_cast<Axis>( m_axisComboBox->currentIndex() );
}

void Lanelet2MapDialog::chooseOutputPath()
{
	const QString fileName = QFileDialog::getSaveFileName(
		this,
		"Save Lanelet2 map",
		outputPath().isEmpty() ? QStringLiteral( "lanelet2_demo.osm" ) : outputPath(),
		"Lanelet2 map (*.osm)" );

	if ( !fileName.isEmpty() )
	{
		m_outputPathEdit->setText( fileName );
	}
}

void Lanelet2MapDialog::validateAndAccept()
{
	if ( outputPath().isEmpty() )
	{
		QMessageBox::warning( this, "Lanelet2 Map Demo", "Choose an output `.osm` file first." );
		return;
	}

	accept();
}
