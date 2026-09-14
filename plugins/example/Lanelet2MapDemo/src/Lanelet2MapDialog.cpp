#include "Lanelet2MapDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStringConverter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <array>
#include <ccMainAppInterface.h>
#include <ccPointCloud.h>
#include <cmath>

namespace
{
	struct GlobalPoint
	{
		double x;
		double y;
		double z;
	};

	struct GeodeticPoint
	{
		double latitude;
		double longitude;
		double altitude;
	};

	struct EcefPoint
	{
		double x;
		double y;
		double z;
	};

	constexpr double c_pi      = 3.14159265358979323846;
	constexpr double c_wgs84A  = 6378137.0;
	constexpr double c_wgs84F  = 1.0 / 298.257223563;
	constexpr double c_wgs84E2 = c_wgs84F * (2.0 - c_wgs84F);

	double degreesToRadians(double value)
	{
		return value * c_pi / 180.0;
	}

	double radiansToDegrees(double value)
	{
		return value * 180.0 / c_pi;
	}

	EcefPoint geodeticToEcef(const GeodeticPoint& point)
	{
		const double latitudeRadians     = degreesToRadians(point.latitude);
		const double longitudeRadians    = degreesToRadians(point.longitude);
		const double sinLatitude         = std::sin(latitudeRadians);
		const double cosLatitude         = std::cos(latitudeRadians);
		const double sinLongitude        = std::sin(longitudeRadians);
		const double cosLongitude        = std::cos(longitudeRadians);
		const double primeVerticalRadius = c_wgs84A / std::sqrt(1.0 - c_wgs84E2 * sinLatitude * sinLatitude);

		return {
		    (primeVerticalRadius + point.altitude) * cosLatitude * cosLongitude,
		    (primeVerticalRadius + point.altitude) * cosLatitude * sinLongitude,
		    (primeVerticalRadius * (1.0 - c_wgs84E2) + point.altitude) * sinLatitude};
	}

	GeodeticPoint ecefToGeodetic(const EcefPoint& point)
	{
		const double semiMinorAxis             = c_wgs84A * (1.0 - c_wgs84F);
		const double secondEccentricitySquared = (c_wgs84A * c_wgs84A - semiMinorAxis * semiMinorAxis)
		                                         / (semiMinorAxis * semiMinorAxis);
		const double p         = std::sqrt(point.x * point.x + point.y * point.y);
		const double theta     = std::atan2(point.z * c_wgs84A, p * semiMinorAxis);
		const double sinTheta  = std::sin(theta);
		const double cosTheta  = std::cos(theta);
		const double longitude = std::atan2(point.y, point.x);
		const double latitude  = std::atan2(
            point.z + secondEccentricitySquared * semiMinorAxis * sinTheta * sinTheta * sinTheta,
            p - c_wgs84E2 * c_wgs84A * cosTheta * cosTheta * cosTheta);
		const double sinLatitude         = std::sin(latitude);
		const double primeVerticalRadius = c_wgs84A / std::sqrt(1.0 - c_wgs84E2 * sinLatitude * sinLatitude);
		const double altitude            = p / std::cos(latitude) - primeVerticalRadius;

		return {radiansToDegrees(latitude), radiansToDegrees(longitude), altitude};
	}

	GeodeticPoint localEnuToGeodetic(const GlobalPoint& point, const GeodeticPoint& origin)
	{
		const double    latitudeRadians  = degreesToRadians(origin.latitude);
		const double    longitudeRadians = degreesToRadians(origin.longitude);
		const double    sinLatitude      = std::sin(latitudeRadians);
		const double    cosLatitude      = std::cos(latitudeRadians);
		const double    sinLongitude     = std::sin(longitudeRadians);
		const double    cosLongitude     = std::cos(longitudeRadians);
		const EcefPoint originEcef       = geodeticToEcef(origin);
		const EcefPoint ecef{
		    originEcef.x - sinLongitude * point.x - sinLatitude * cosLongitude * point.y + cosLatitude * cosLongitude * point.z,
		    originEcef.y + cosLongitude * point.x - sinLatitude * sinLongitude * point.y + cosLatitude * sinLongitude * point.z,
		    originEcef.z + cosLatitude * point.y + sinLatitude * point.z};

		return ecefToGeodetic(ecef);
	}

	QString formatNumber(double value, int decimals)
	{
		return QString::number(value, 'f', decimals);
	}

	QString axisToText(Lanelet2MapDialog::Axis axis)
	{
		switch (axis)
		{
		case Lanelet2MapDialog::Axis::X:
			return "x";
		case Lanelet2MapDialog::Axis::Y:
			return "y";
		case Lanelet2MapDialog::Axis::Auto:
		default:
			return "auto";
		}
	}

	QString escapeXml(const QString& value)
	{
		QString escaped = value.toHtmlEscaped();
		escaped.replace('\'', "&apos;");
		return escaped;
	}

	QTableWidgetItem* makeReadOnlyItem(const QString& text)
	{
		auto* item = new QTableWidgetItem(text);
		item->setFlags(item->flags() & ~Qt::ItemIsEditable);
		return item;
	}

	double boundsWidth(const Lanelet2MapDialog::Bounds& bounds)
	{
		return std::max(0.0, bounds.maxX - bounds.minX);
	}

	double boundsHeight(const Lanelet2MapDialog::Bounds& bounds)
	{
		return std::max(0.0, bounds.maxY - bounds.minY);
	}

	double boundsCenterX(const Lanelet2MapDialog::Bounds& bounds)
	{
		return (bounds.minX + bounds.maxX) / 2.0;
	}

	double boundsCenterY(const Lanelet2MapDialog::Bounds& bounds)
	{
		return (bounds.minY + bounds.maxY) / 2.0;
	}

	double boundsCenterZ(const Lanelet2MapDialog::Bounds& bounds)
	{
		return (bounds.minZ + bounds.maxZ) / 2.0;
	}

	std::array<GlobalPoint, 2> buildBoundaryPointPair(const Lanelet2MapDialog::LaneRecord& lane,
	                                                  Lanelet2MapDialog::Axis              axis,
	                                                  double                               ratio)
	{
		const bool   alongX    = (axis == Lanelet2MapDialog::Axis::X);
		const double length    = std::max(alongX ? boundsWidth(lane.bounds) : boundsHeight(lane.bounds), lane.laneWidth);
		const double halfWidth = lane.laneWidth / 2.0;
		const double offset    = -0.5 * length + ratio * length;
		const double centerX   = boundsCenterX(lane.bounds);
		const double centerY   = boundsCenterY(lane.bounds);
		const double centerZ   = boundsCenterZ(lane.bounds);

		if (alongX)
		{
			return {GlobalPoint{centerX + offset, centerY + halfWidth, centerZ},
			        GlobalPoint{centerX + offset, centerY - halfWidth, centerZ}};
		}

		return {GlobalPoint{centerX - halfWidth, centerY + offset, centerZ},
		        GlobalPoint{centerX + halfWidth, centerY + offset, centerZ}};
	}

	void writeNode(QTextStream& stream, qint64 nodeId, const GlobalPoint& point, const GeodeticPoint& origin)
	{
		const GeodeticPoint geodetic = localEnuToGeodetic(point, origin);

		stream << "  <node id='" << nodeId << "' visible='true' version='1' lat='" << formatNumber(geodetic.latitude, 11)
		       << "' lon='" << formatNumber(geodetic.longitude, 11) << "'>\n";
		stream << "    <tag k='ele' v='" << formatNumber(geodetic.altitude, 3) << "' />\n";
		stream << "    <tag k='local_x' v='" << formatNumber(point.x, 3) << "' />\n";
		stream << "    <tag k='local_y' v='" << formatNumber(point.y, 3) << "' />\n";
		stream << "    <tag k='local_z' v='" << formatNumber(point.z, 3) << "' />\n";
		stream << "  </node>\n";
	}
} // namespace

Lanelet2MapDialog::Lanelet2MapDialog(ccMainAppInterface* app, QWidget* parent)
    : ccOverlayDialog(parent)
    , m_app(app)
    , m_currentCloud(nullptr)
    , m_nextCatalogId(1)
    , m_currentCloudEdit(new QLineEdit(this))
    , m_outputPathEdit(new QLineEdit(this))
    , m_originLatitudeSpinBox(new QDoubleSpinBox(this))
    , m_originLongitudeSpinBox(new QDoubleSpinBox(this))
    , m_originAltitudeSpinBox(new QDoubleSpinBox(this))
    , m_laneWidthSpinBox(new QDoubleSpinBox(this))
    , m_sampleCountSpinBox(new QSpinBox(this))
    , m_axisComboBox(new QComboBox(this))
    , m_objectSubtypeComboBox(new QComboBox(this))
    , m_tabWidget(new QTabWidget(this))
    , m_lanesTable(new QTableWidget(this))
    , m_objectsTable(new QTableWidget(this))
    , m_addLaneButton(new QPushButton("Add lane from selection", this))
    , m_addObjectButton(new QPushButton("Add object from selection", this))
    , m_removeButton(new QPushButton("Remove selected", this))
    , m_exportButton(new QPushButton("Export catalog", this))
    , m_clearButton(new QPushButton("Clear catalog", this))
{
	setWindowTitle("Lanelet2 Catalog Demo");
	setWindowFlags(Qt::Tool);
	setMinimumSize(900, 620);

	auto* mainLayout = new QVBoxLayout(this);

	auto* summaryGroup  = new QGroupBox("Current point-cloud selection", this);
	auto* summaryLayout = new QVBoxLayout(summaryGroup);
	m_currentCloudEdit->setReadOnly(true);
	summaryLayout->addWidget(m_currentCloudEdit);
	mainLayout->addWidget(summaryGroup);

	auto* settingsGroup  = new QGroupBox("Catalog defaults and export settings", this);
	auto* settingsLayout = new QFormLayout(settingsGroup);
	auto* outputLayout   = new QHBoxLayout();
	auto* outputWidget   = new QWidget(settingsGroup);
	auto* browseButton   = new QPushButton("Browse…", settingsGroup);
	outputLayout->setContentsMargins(0, 0, 0, 0);
	outputLayout->addWidget(m_outputPathEdit, 1);
	outputLayout->addWidget(browseButton);
	outputWidget->setLayout(outputLayout);
	settingsLayout->addRow("Output `.osm` file", outputWidget);

	m_originLatitudeSpinBox->setDecimals(8);
	m_originLatitudeSpinBox->setRange(-90.0, 90.0);
	settingsLayout->addRow("Origin latitude", m_originLatitudeSpinBox);

	m_originLongitudeSpinBox->setDecimals(8);
	m_originLongitudeSpinBox->setRange(-180.0, 180.0);
	settingsLayout->addRow("Origin longitude", m_originLongitudeSpinBox);

	m_originAltitudeSpinBox->setDecimals(3);
	m_originAltitudeSpinBox->setRange(-10000.0, 100000.0);
	settingsLayout->addRow("Origin altitude (m)", m_originAltitudeSpinBox);

	m_laneWidthSpinBox->setDecimals(3);
	m_laneWidthSpinBox->setRange(0.1, 1000.0);
	m_laneWidthSpinBox->setValue(3.5);
	settingsLayout->addRow("New lane width (m)", m_laneWidthSpinBox);

	m_sampleCountSpinBox->setRange(2, 200);
	m_sampleCountSpinBox->setValue(2);
	settingsLayout->addRow("New lane sample count", m_sampleCountSpinBox);

	m_axisComboBox->addItem("Auto (longest XY extent)");
	m_axisComboBox->addItem("X axis");
	m_axisComboBox->addItem("Y axis");
	settingsLayout->addRow("New lane direction", m_axisComboBox);

	m_objectSubtypeComboBox->addItems({"building", "keepout", "traffic_sign", "vegetation"});
	settingsLayout->addRow("New object subtype", m_objectSubtypeComboBox);

	mainLayout->addWidget(settingsGroup);

	m_lanesTable->setColumnCount(6);
	m_lanesTable->setHorizontalHeaderLabels({"ID", "Name", "Source cloud", "Axis", "Width (m)", "Samples"});
	m_lanesTable->horizontalHeader()->setStretchLastSection(true);
	m_lanesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_lanesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_lanesTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_lanesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

	m_objectsTable->setColumnCount(5);
	m_objectsTable->setHorizontalHeaderLabels({"ID", "Name", "Subtype", "Source cloud", "Footprint (m)"});
	m_objectsTable->horizontalHeader()->setStretchLastSection(true);
	m_objectsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_objectsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_objectsTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_objectsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

	m_tabWidget->addTab(m_lanesTable, "Lanes");
	m_tabWidget->addTab(m_objectsTable, "Objects");
	mainLayout->addWidget(m_tabWidget, 1);

	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addWidget(m_addLaneButton);
	buttonLayout->addWidget(m_addObjectButton);
	buttonLayout->addWidget(m_removeButton);
	buttonLayout->addWidget(m_clearButton);
	buttonLayout->addStretch(1);
	buttonLayout->addWidget(m_exportButton);
	auto* closeButton = new QPushButton("Close", this);
	buttonLayout->addWidget(closeButton);
	mainLayout->addLayout(buttonLayout);

	auto* noteLabel = new QLabel(
	    "Catalog entries are created from the selected cloud's global/original coordinates. "
	    "Reuse the same LocalCartesian origin when loading the exported Lanelet2 map.",
	    this);
	noteLabel->setWordWrap(true);
	mainLayout->addWidget(noteLabel);

	connect(browseButton, &QPushButton::clicked, this, &Lanelet2MapDialog::browseOutputPath);
	connect(m_addLaneButton, &QPushButton::clicked, this, &Lanelet2MapDialog::addLaneFromCurrentCloud);
	connect(m_addObjectButton, &QPushButton::clicked, this, &Lanelet2MapDialog::addObjectFromCurrentCloud);
	connect(m_removeButton, &QPushButton::clicked, this, &Lanelet2MapDialog::removeSelectedEntry);
	connect(m_clearButton, &QPushButton::clicked, this, &Lanelet2MapDialog::clearCatalog);
	connect(m_exportButton, &QPushButton::clicked, this, &Lanelet2MapDialog::exportCatalog);
	connect(closeButton, &QPushButton::clicked, this, [this]()
	        { stop(false); });

	setCurrentCloud(nullptr);
	refreshTables();
}

bool Lanelet2MapDialog::start()
{
	if (started())
	{
		return false;
	}

	return ccOverlayDialog::start();
}

void Lanelet2MapDialog::stop(bool accepted)
{
	ccOverlayDialog::stop(accepted);
}

void Lanelet2MapDialog::setCurrentCloud(ccPointCloud* cloud)
{
	m_currentCloud = cloud;
	m_currentCloudEdit->setText(buildCloudSummary(cloud));

	const bool hasCloud = (m_currentCloud != nullptr);
	m_addLaneButton->setEnabled(hasCloud);
	m_addObjectButton->setEnabled(hasCloud);
}

void Lanelet2MapDialog::addLaneFromCurrentCloud()
{
	QString errorMessage;
	Bounds  bounds;
	if (!populateBounds(m_currentCloud, bounds, &errorMessage))
	{
		QMessageBox::warning(this, "Lanelet2 Catalog Demo", errorMessage);
		return;
	}

	LaneRecord lane;
	lane.id              = m_nextCatalogId++;
	lane.name            = QString("Lane %1").arg(lane.id);
	lane.sourceCloudName = m_currentCloud->getName();
	lane.bounds          = bounds;
	lane.laneWidth       = m_laneWidthSpinBox->value();
	lane.sampleCount     = m_sampleCountSpinBox->value();
	lane.axis            = currentAxis();

	m_lanes.push_back(lane);
	refreshTables();
}

void Lanelet2MapDialog::addObjectFromCurrentCloud()
{
	QString errorMessage;
	Bounds  bounds;
	if (!populateBounds(m_currentCloud, bounds, &errorMessage))
	{
		QMessageBox::warning(this, "Lanelet2 Catalog Demo", errorMessage);
		return;
	}

	ObjectRecord object;
	object.id              = m_nextCatalogId++;
	object.name            = QString("Object %1").arg(object.id);
	object.subtype         = m_objectSubtypeComboBox->currentText();
	object.sourceCloudName = m_currentCloud->getName();
	object.bounds          = bounds;

	m_objects.push_back(object);
	refreshTables();
}

void Lanelet2MapDialog::removeSelectedEntry()
{
	if (m_tabWidget->currentWidget() == m_lanesTable)
	{
		const int row = m_lanesTable->currentRow();
		if (row >= 0 && row < m_lanes.size())
		{
			m_lanes.removeAt(row);
		}
	}
	else
	{
		const int row = m_objectsTable->currentRow();
		if (row >= 0 && row < m_objects.size())
		{
			m_objects.removeAt(row);
		}
	}

	refreshTables();
}

void Lanelet2MapDialog::exportCatalog()
{
	const QString outputPath = m_outputPathEdit->text().trimmed();
	if (outputPath.isEmpty())
	{
		QMessageBox::warning(this, "Lanelet2 Catalog Demo", "Choose an output `.osm` file first.");
		return;
	}

	if (m_lanes.isEmpty() && m_objects.isEmpty())
	{
		QMessageBox::warning(this, "Lanelet2 Catalog Demo", "Add at least one lane or object before exporting.");
		return;
	}

	QString errorMessage;
	if (!writeLaneletMap(outputPath, errorMessage))
	{
		QMessageBox::critical(this, "Lanelet2 Catalog Demo", errorMessage);
		return;
	}

	if (m_app)
	{
		m_app->dispToConsole(
		    QString("Lanelet2 catalog exported to `%1`. Reuse the configured LocalCartesian origin when loading it.").arg(outputPath),
		    ccMainAppInterface::STD_CONSOLE_MESSAGE);
	}
}

void Lanelet2MapDialog::browseOutputPath()
{
	const QString fileName = QFileDialog::getSaveFileName(
	    this,
	    "Save Lanelet2 map catalog",
	    m_outputPathEdit->text().trimmed().isEmpty() ? QStringLiteral("lanelet2_catalog.osm") : m_outputPathEdit->text().trimmed(),
	    "Lanelet2 map (*.osm)");

	if (!fileName.isEmpty())
	{
		m_outputPathEdit->setText(fileName);
	}
}

void Lanelet2MapDialog::clearCatalog()
{
	if (m_lanes.isEmpty() && m_objects.isEmpty())
	{
		return;
	}

	if (QMessageBox::question(this, "Lanelet2 Catalog Demo", "Clear all catalog entries?") != QMessageBox::Yes)
	{
		return;
	}

	m_lanes.clear();
	m_objects.clear();
	refreshTables();
}

void Lanelet2MapDialog::refreshTables()
{
	m_lanesTable->setRowCount(m_lanes.size());
	for (int row = 0; row < m_lanes.size(); ++row)
	{
		const LaneRecord& lane = m_lanes[row];
		m_lanesTable->setItem(row, 0, makeReadOnlyItem(QString::number(lane.id)));
		m_lanesTable->setItem(row, 1, makeReadOnlyItem(lane.name));
		m_lanesTable->setItem(row, 2, makeReadOnlyItem(lane.sourceCloudName));
		m_lanesTable->setItem(row, 3, makeReadOnlyItem(axisToText(lane.axis)));
		m_lanesTable->setItem(row, 4, makeReadOnlyItem(formatNumber(lane.laneWidth, 3)));
		m_lanesTable->setItem(row, 5, makeReadOnlyItem(QString::number(lane.sampleCount)));
	}

	m_objectsTable->setRowCount(m_objects.size());
	for (int row = 0; row < m_objects.size(); ++row)
	{
		const ObjectRecord& object    = m_objects[row];
		const QString       footprint = QString("%1 x %2")
		                              .arg(formatNumber(boundsWidth(object.bounds), 3))
		                              .arg(formatNumber(boundsHeight(object.bounds), 3));
		m_objectsTable->setItem(row, 0, makeReadOnlyItem(QString::number(object.id)));
		m_objectsTable->setItem(row, 1, makeReadOnlyItem(object.name));
		m_objectsTable->setItem(row, 2, makeReadOnlyItem(object.subtype));
		m_objectsTable->setItem(row, 3, makeReadOnlyItem(object.sourceCloudName));
		m_objectsTable->setItem(row, 4, makeReadOnlyItem(footprint));
	}

	m_removeButton->setEnabled(!m_lanes.isEmpty() || !m_objects.isEmpty());
	m_exportButton->setEnabled(!m_lanes.isEmpty() || !m_objects.isEmpty());
	m_clearButton->setEnabled(!m_lanes.isEmpty() || !m_objects.isEmpty());
}

QString Lanelet2MapDialog::buildCloudSummary(ccPointCloud* cloud) const
{
	if (!cloud)
	{
		return "Select one point cloud in CloudCompare to add lane or object entries.";
	}

	Bounds bounds;
	if (!populateBounds(cloud, bounds))
	{
		return QString("%1 | invalid global bounding box").arg(cloud->getName());
	}

	const CCVector3d shift = cloud->getGlobalShift();
	return QString("%1 | points=%2 | global min=(%3, %4, %5) | global max=(%6, %7, %8) | shift=(%9, %10, %11) | scale=%12")
	    .arg(cloud->getName())
	    .arg(cloud->size())
	    .arg(bounds.minX, 0, 'f', 3)
	    .arg(bounds.minY, 0, 'f', 3)
	    .arg(bounds.minZ, 0, 'f', 3)
	    .arg(bounds.maxX, 0, 'f', 3)
	    .arg(bounds.maxY, 0, 'f', 3)
	    .arg(bounds.maxZ, 0, 'f', 3)
	    .arg(shift.x, 0, 'f', 3)
	    .arg(shift.y, 0, 'f', 3)
	    .arg(shift.z, 0, 'f', 3)
	    .arg(cloud->getGlobalScale(), 0, 'f', 6);
}

bool Lanelet2MapDialog::populateBounds(ccPointCloud* cloud, Bounds& bounds, QString* errorMessage) const
{
	if (!cloud)
	{
		if (errorMessage)
		{
			*errorMessage = "Select one point cloud first.";
		}
		return false;
	}

	ccHObject::GlobalBoundingBox globalBox = cloud->getOwnGlobalBB(false);
	if (!globalBox.isValid())
	{
		if (errorMessage)
		{
			*errorMessage = "The selected point cloud does not have a valid global bounding box.";
		}
		return false;
	}

	const CCVector3d minCorner = globalBox.minCorner();
	const CCVector3d maxCorner = globalBox.maxCorner();
	bounds.minX                = minCorner.x;
	bounds.minY                = minCorner.y;
	bounds.minZ                = minCorner.z;
	bounds.maxX                = maxCorner.x;
	bounds.maxY                = maxCorner.y;
	bounds.maxZ                = maxCorner.z;
	return true;
}

Lanelet2MapDialog::Axis Lanelet2MapDialog::currentAxis() const
{
	return static_cast<Axis>(m_axisComboBox->currentIndex());
}

bool Lanelet2MapDialog::writeLaneletMap(const QString& outputPath, QString& errorMessage) const
{
	QSaveFile file(outputPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		errorMessage = QString("Failed to open `%1` for writing.").arg(outputPath);
		return false;
	}

	QTextStream stream(&file);
	stream.setEncoding(QStringConverter::Utf8);

	const GeodeticPoint origin{
	    m_originLatitudeSpinBox->value(),
	    m_originLongitudeSpinBox->value(),
	    m_originAltitudeSpinBox->value()};

	stream << "<?xml version='1.0' encoding='UTF-8'?>\n";
	stream << "<!-- Generated by CloudCompare Lanelet2 Catalog Demo. "
	       << "Load with a LocalCartesian projector that uses origin lat=" << formatNumber(origin.latitude, 8)
	       << ", lon=" << formatNumber(origin.longitude, 8)
	       << ", alt=" << formatNumber(origin.altitude, 3) << ". -->\n";
	stream << "<osm version='0.6' generator='CloudCompare Lanelet2 Catalog Demo'>\n";

	qint64 nextId = -1;

	for (const LaneRecord& lane : m_lanes)
	{
		const Axis resolvedAxis = (lane.axis == Axis::Auto
		                               ? (boundsWidth(lane.bounds) >= boundsHeight(lane.bounds) ? Axis::X : Axis::Y)
		                               : lane.axis);

		QList<qint64> leftNodeIds;
		QList<qint64> rightNodeIds;
		for (int index = 0; index < std::max(2, lane.sampleCount); ++index)
		{
			const double ratio        = (lane.sampleCount <= 1 ? 0.0 : static_cast<double>(index) / static_cast<double>(lane.sampleCount - 1));
			const auto   boundaryPair = buildBoundaryPointPair(lane, resolvedAxis, ratio);
			for (int boundaryIndex = 0; boundaryIndex < 2; ++boundaryIndex)
			{
				const qint64 nodeId = nextId--;
				writeNode(stream, nodeId, boundaryPair[boundaryIndex], origin);
				(boundaryIndex == 0 ? leftNodeIds : rightNodeIds).push_back(nodeId);
			}
		}

		const qint64 leftWayId = nextId--;
		stream << "  <way id='" << leftWayId << "' visible='true' version='1'>\n";
		for (qint64 nodeId : leftNodeIds)
		{
			stream << "    <nd ref='" << nodeId << "' />\n";
		}
		stream << "    <tag k='type' v='line_thin' />\n";
		stream << "    <tag k='subtype' v='solid' />\n";
		stream << "  </way>\n";

		const qint64 rightWayId = nextId--;
		stream << "  <way id='" << rightWayId << "' visible='true' version='1'>\n";
		for (qint64 nodeId : rightNodeIds)
		{
			stream << "    <nd ref='" << nodeId << "' />\n";
		}
		stream << "    <tag k='type' v='line_thin' />\n";
		stream << "    <tag k='subtype' v='solid' />\n";
		stream << "  </way>\n";

		const qint64 relationId = nextId--;
		stream << "  <relation id='" << relationId << "' visible='true' version='1'>\n";
		stream << "    <member type='way' ref='" << leftWayId << "' role='left' />\n";
		stream << "    <member type='way' ref='" << rightWayId << "' role='right' />\n";
		stream << "    <tag k='location' v='urban' />\n";
		stream << "    <tag k='one_way' v='yes' />\n";
		stream << "    <tag k='subtype' v='road' />\n";
		stream << "    <tag k='type' v='lanelet' />\n";
		stream << "    <tag k='name' v='" << escapeXml(lane.name) << "' />\n";
		stream << "    <tag k='source:cloudcompare:cloud' v='" << escapeXml(lane.sourceCloudName) << "' />\n";
		stream << "    <tag k='source:cloudcompare:axis' v='" << axisToText(resolvedAxis) << "' />\n";
		stream << "    <tag k='source:cloudcompare:frame' v='local_cartesian_enu' />\n";
		stream << "  </relation>\n";
	}

	for (const ObjectRecord& object : m_objects)
	{
		QList<qint64>                    nodeIds;
		const std::array<GlobalPoint, 5> corners{
		    GlobalPoint{object.bounds.minX, object.bounds.minY, object.bounds.minZ},
		    GlobalPoint{object.bounds.maxX, object.bounds.minY, object.bounds.minZ},
		    GlobalPoint{object.bounds.maxX, object.bounds.maxY, object.bounds.minZ},
		    GlobalPoint{object.bounds.minX, object.bounds.maxY, object.bounds.minZ},
		    GlobalPoint{object.bounds.minX, object.bounds.minY, object.bounds.minZ}};

		for (const GlobalPoint& corner : corners)
		{
			const qint64 nodeId = nextId--;
			writeNode(stream, nodeId, corner, origin);
			nodeIds.push_back(nodeId);
		}

		const qint64 wayId = nextId--;
		stream << "  <way id='" << wayId << "' visible='true' version='1'>\n";
		for (qint64 nodeId : nodeIds)
		{
			stream << "    <nd ref='" << nodeId << "' />\n";
		}
		stream << "    <tag k='area' v='yes' />\n";
		stream << "  </way>\n";

		const qint64 relationId = nextId--;
		stream << "  <relation id='" << relationId << "' visible='true' version='1'>\n";
		stream << "    <member type='way' ref='" << wayId << "' role='outer' />\n";
		stream << "    <tag k='type' v='multipolygon' />\n";
		stream << "    <tag k='subtype' v='" << escapeXml(object.subtype) << "' />\n";
		stream << "    <tag k='name' v='" << escapeXml(object.name) << "' />\n";
		stream << "    <tag k='source:cloudcompare:cloud' v='" << escapeXml(object.sourceCloudName) << "' />\n";
		stream << "    <tag k='source:cloudcompare:frame' v='local_cartesian_enu' />\n";
		stream << "  </relation>\n";
	}

	stream << "</osm>\n";
	if (!file.commit())
	{
		errorMessage = QString("Failed to finalize `%1`.").arg(outputPath);
		return false;
	}

	return true;
}
