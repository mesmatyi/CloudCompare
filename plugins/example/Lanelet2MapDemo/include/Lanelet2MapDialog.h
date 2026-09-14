#pragma once

#include <QList>
#include <QString>
#include <ccOverlayDialog.h>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class ccMainAppInterface;
class ccPointCloud;

class Lanelet2MapDialog : public ccOverlayDialog
{
	Q_OBJECT

  public:
	enum class Axis
	{
		Auto = 0,
		X,
		Y
	};

	struct Bounds
	{
		double minX = 0.0;
		double minY = 0.0;
		double minZ = 0.0;
		double maxX = 0.0;
		double maxY = 0.0;
		double maxZ = 0.0;
	};

	struct LaneRecord
	{
		int     id = 0;
		QString name;
		QString sourceCloudName;
		Bounds  bounds;
		double  laneWidth   = 3.5;
		int     sampleCount = 2;
		Axis    axis        = Axis::Auto;
	};

	struct ObjectRecord
	{
		int     id = 0;
		QString name;
		QString subtype;
		QString sourceCloudName;
		Bounds  bounds;
	};

	explicit Lanelet2MapDialog(ccMainAppInterface* app, QWidget* parent = nullptr);

	bool start() override;
	void stop(bool accepted) override;

	void setCurrentCloud(ccPointCloud* cloud);

  private:
	void addLaneFromCurrentCloud();
	void addObjectFromCurrentCloud();
	void removeSelectedEntry();
	void exportCatalog();
	void browseOutputPath();
	void clearCatalog();

	void    refreshTables();
	QString buildCloudSummary(ccPointCloud* cloud) const;
	bool    populateBounds(ccPointCloud* cloud, Bounds& bounds, QString* errorMessage = nullptr) const;
	Axis    currentAxis() const;
	bool    writeLaneletMap(const QString& outputPath, QString& errorMessage) const;

	ccMainAppInterface* m_app;
	ccPointCloud*       m_currentCloud;
	int                 m_nextCatalogId;
	QLineEdit*          m_currentCloudEdit;
	QLineEdit*          m_outputPathEdit;
	QDoubleSpinBox*     m_originLatitudeSpinBox;
	QDoubleSpinBox*     m_originLongitudeSpinBox;
	QDoubleSpinBox*     m_originAltitudeSpinBox;
	QDoubleSpinBox*     m_laneWidthSpinBox;
	QSpinBox*           m_sampleCountSpinBox;
	QComboBox*          m_axisComboBox;
	QComboBox*          m_objectSubtypeComboBox;
	QTabWidget*         m_tabWidget;
	QTableWidget*       m_lanesTable;
	QTableWidget*       m_objectsTable;
	QPushButton*        m_addLaneButton;
	QPushButton*        m_addObjectButton;
	QPushButton*        m_removeButton;
	QPushButton*        m_exportButton;
	QPushButton*        m_clearButton;
	QList<LaneRecord>   m_lanes;
	QList<ObjectRecord> m_objects;
};
