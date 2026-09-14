#pragma once

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

class Lanelet2MapDialog : public QDialog
{
	Q_OBJECT

public:
	enum class Axis
	{
		Auto = 0,
		X,
		Y
	};

	explicit Lanelet2MapDialog( QWidget* parent = nullptr );

	void setCloudSummary( const QString& summary );

	QString outputPath() const;
	double originLatitude() const;
	double originLongitude() const;
	double originAltitude() const;
	double laneWidth() const;
	int sampleCount() const;
	Axis axis() const;

private:
	void chooseOutputPath();
	void validateAndAccept();

	QLineEdit* m_summaryEdit;
	QLineEdit* m_outputPathEdit;
	QDoubleSpinBox* m_originLatitudeSpinBox;
	QDoubleSpinBox* m_originLongitudeSpinBox;
	QDoubleSpinBox* m_originAltitudeSpinBox;
	QDoubleSpinBox* m_laneWidthSpinBox;
	QSpinBox* m_sampleCountSpinBox;
	QComboBox* m_axisComboBox;
};
