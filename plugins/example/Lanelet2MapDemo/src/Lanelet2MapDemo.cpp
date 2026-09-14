#include "Lanelet2MapDemo.h"

#include "Lanelet2MapDialog.h"

#include <ccHObjectCaster.h>
#include <ccPointCloud.h>

#include <QAction>
#include <QMessageBox>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>
#include <array>
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

constexpr double c_pi = 3.14159265358979323846;
constexpr double c_wgs84A = 6378137.0;
constexpr double c_wgs84F = 1.0 / 298.257223563;
constexpr double c_wgs84E2 = c_wgs84F * ( 2.0 - c_wgs84F );

double degreesToRadians( double value )
{
	return value * c_pi / 180.0;
}

double radiansToDegrees( double value )
{
	return value * 180.0 / c_pi;
}

EcefPoint geodeticToEcef( const GeodeticPoint& point )
{
	const double latitudeRadians = degreesToRadians( point.latitude );
	const double longitudeRadians = degreesToRadians( point.longitude );
	const double sinLatitude = std::sin( latitudeRadians );
	const double cosLatitude = std::cos( latitudeRadians );
	const double sinLongitude = std::sin( longitudeRadians );
	const double cosLongitude = std::cos( longitudeRadians );
	const double primeVerticalRadius = c_wgs84A / std::sqrt( 1.0 - c_wgs84E2 * sinLatitude * sinLatitude );

	return {
		( primeVerticalRadius + point.altitude ) * cosLatitude * cosLongitude,
		( primeVerticalRadius + point.altitude ) * cosLatitude * sinLongitude,
		( primeVerticalRadius * ( 1.0 - c_wgs84E2 ) + point.altitude ) * sinLatitude
	};
}

GeodeticPoint ecefToGeodetic( const EcefPoint& point )
{
	const double semiMinorAxis = c_wgs84A * ( 1.0 - c_wgs84F );
	const double secondEccentricitySquared = ( c_wgs84A * c_wgs84A - semiMinorAxis * semiMinorAxis )
		/ ( semiMinorAxis * semiMinorAxis );
	const double p = std::sqrt( point.x * point.x + point.y * point.y );
	const double theta = std::atan2( point.z * c_wgs84A, p * semiMinorAxis );
	const double sinTheta = std::sin( theta );
	const double cosTheta = std::cos( theta );
	const double longitude = std::atan2( point.y, point.x );
	const double latitude = std::atan2(
		point.z + secondEccentricitySquared * semiMinorAxis * sinTheta * sinTheta * sinTheta,
		p - c_wgs84E2 * c_wgs84A * cosTheta * cosTheta * cosTheta );
	const double sinLatitude = std::sin( latitude );
	const double primeVerticalRadius = c_wgs84A / std::sqrt( 1.0 - c_wgs84E2 * sinLatitude * sinLatitude );
	const double altitude = p / std::cos( latitude ) - primeVerticalRadius;

	return { radiansToDegrees( latitude ), radiansToDegrees( longitude ), altitude };
}

GeodeticPoint localEnuToGeodetic( const GlobalPoint& point, const GeodeticPoint& origin )
{
	const double latitudeRadians = degreesToRadians( origin.latitude );
	const double longitudeRadians = degreesToRadians( origin.longitude );
	const double sinLatitude = std::sin( latitudeRadians );
	const double cosLatitude = std::cos( latitudeRadians );
	const double sinLongitude = std::sin( longitudeRadians );
	const double cosLongitude = std::cos( longitudeRadians );

	const EcefPoint originEcef = geodeticToEcef( origin );
	const EcefPoint ecef {
		originEcef.x - sinLongitude * point.x - sinLatitude * cosLongitude * point.y + cosLatitude * cosLongitude * point.z,
		originEcef.y + cosLongitude * point.x - sinLatitude * sinLongitude * point.y + cosLatitude * sinLongitude * point.z,
		originEcef.z + cosLatitude * point.y + sinLatitude * point.z
	};

	return ecefToGeodetic( ecef );
}

QString formatNodeAttribute( double value, int decimals )
{
	return QString::number( value, 'f', decimals );
}

QString xmlEscape( const QString& value )
{
	QString escaped = value.toHtmlEscaped();
	escaped.replace( '\'', "&apos;" );
	return escaped;
}

QString buildCloudSummary( ccPointCloud* cloud, const CCVector3d& minCorner, const CCVector3d& maxCorner )
{
	const CCVector3d shift = cloud->getGlobalShift();
	return QString( "%1 | points=%2 | global min=(%3, %4, %5) | global max=(%6, %7, %8) | shift=(%9, %10, %11) | scale=%12" )
		.arg( cloud->getName() )
		.arg( cloud->size() )
		.arg( minCorner.x, 0, 'f', 3 )
		.arg( minCorner.y, 0, 'f', 3 )
		.arg( minCorner.z, 0, 'f', 3 )
		.arg( maxCorner.x, 0, 'f', 3 )
		.arg( maxCorner.y, 0, 'f', 3 )
		.arg( maxCorner.z, 0, 'f', 3 )
		.arg( shift.x, 0, 'f', 3 )
		.arg( shift.y, 0, 'f', 3 )
		.arg( shift.z, 0, 'f', 3 )
		.arg( cloud->getGlobalScale(), 0, 'f', 6 );
}

std::array<GlobalPoint, 2> buildBoundaryPointPair( const CCVector3d& center,
                                                   double length,
                                                   double halfWidth,
                                                   bool alongX,
                                                   double ratio )
{
	const double offset = -0.5 * length + ratio * length;

	if ( alongX )
	{
		return { GlobalPoint { center.x + offset, center.y + halfWidth, center.z },
			     GlobalPoint { center.x + offset, center.y - halfWidth, center.z } };
	}

	return { GlobalPoint { center.x - halfWidth, center.y + offset, center.z },
		     GlobalPoint { center.x + halfWidth, center.y + offset, center.z } };
}

bool writeLaneletMap( const QString& outputPath,
                      ccPointCloud* cloud,
                      const Lanelet2MapDialog& dialog,
                      QString& errorMessage )
{
	const ccHObject::GlobalBoundingBox globalBox = cloud->getOwnGlobalBB( false );
	if ( !globalBox.isValid() )
	{
		errorMessage = "The selected point cloud does not have a valid bounding box.";
		return false;
	}

	const CCVector3d minCorner = globalBox.minCorner();
	const CCVector3d maxCorner = globalBox.maxCorner();
	const CCVector3d center = ( minCorner + maxCorner ) / 2.0;
	const double extentX = std::max( 0.0, maxCorner.x - minCorner.x );
	const double extentY = std::max( 0.0, maxCorner.y - minCorner.y );
	const bool alongX = ( dialog.axis() == Lanelet2MapDialog::Axis::X )
		|| ( dialog.axis() == Lanelet2MapDialog::Axis::Auto && extentX >= extentY );
	const double length = std::max( alongX ? extentX : extentY, dialog.laneWidth() );
	const double halfWidth = dialog.laneWidth() / 2.0;
	const int sampleCount = std::max( 2, dialog.sampleCount() );
	const GeodeticPoint origin { dialog.originLatitude(), dialog.originLongitude(), dialog.originAltitude() };

	QSaveFile file( outputPath );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
	{
		errorMessage = QString( "Failed to open `%1` for writing." ).arg( outputPath );
		return false;
	}

	QTextStream stream( &file );
	stream.setEncoding( QStringConverter::Utf8 );
	stream << "<?xml version='1.0' encoding='UTF-8'?>\n";
	stream << "<!-- Generated by CloudCompare Lanelet2MapDemo. "
	       << "Load with a LocalCartesian projector that uses origin lat=" << formatNodeAttribute( origin.latitude, 8 )
	       << ", lon=" << formatNodeAttribute( origin.longitude, 8 )
	       << ", alt=" << formatNodeAttribute( origin.altitude, 3 ) << ". -->\n";
	stream << "<osm version='0.6' generator='CloudCompare Lanelet2MapDemo'>\n";

	qint64 nextId = -1;
	QList<qint64> leftNodeIds;
	QList<qint64> rightNodeIds;

	for ( int index = 0; index < sampleCount; ++index )
	{
		const double ratio = ( sampleCount == 1 ? 0.0 : static_cast<double>( index ) / static_cast<double>( sampleCount - 1 ) );
		const auto boundaryPair = buildBoundaryPointPair( center, length, halfWidth, alongX, ratio );

		for ( int boundaryIndex = 0; boundaryIndex < 2; ++boundaryIndex )
		{
			const GlobalPoint& boundaryPoint = boundaryPair[boundaryIndex];
			const GeodeticPoint geodeticPoint = localEnuToGeodetic( boundaryPoint, origin );
			const qint64 nodeId = nextId--;

			stream << "  <node id='" << nodeId
			       << "' visible='true' version='1' lat='" << formatNodeAttribute( geodeticPoint.latitude, 11 )
			       << "' lon='" << formatNodeAttribute( geodeticPoint.longitude, 11 ) << "'>\n";
			stream << "    <tag k='ele' v='" << formatNodeAttribute( geodeticPoint.altitude, 3 ) << "' />\n";
			stream << "    <tag k='local_x' v='" << formatNodeAttribute( boundaryPoint.x, 3 ) << "' />\n";
			stream << "    <tag k='local_y' v='" << formatNodeAttribute( boundaryPoint.y, 3 ) << "' />\n";
			stream << "    <tag k='local_z' v='" << formatNodeAttribute( boundaryPoint.z, 3 ) << "' />\n";
			stream << "  </node>\n";

			( boundaryIndex == 0 ? leftNodeIds : rightNodeIds ).push_back( nodeId );
		}
	}

	const qint64 leftWayId = nextId--;
	stream << "  <way id='" << leftWayId << "' visible='true' version='1'>\n";
	for ( qint64 nodeId : leftNodeIds )
	{
		stream << "    <nd ref='" << nodeId << "' />\n";
	}
	stream << "    <tag k='type' v='line_thin' />\n";
	stream << "    <tag k='subtype' v='solid' />\n";
	stream << "  </way>\n";

	const qint64 rightWayId = nextId--;
	stream << "  <way id='" << rightWayId << "' visible='true' version='1'>\n";
	for ( qint64 nodeId : rightNodeIds )
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
	stream << "    <tag k='source' v='CloudCompare Lanelet2MapDemo' />\n";
	stream << "    <tag k='source:cloudcompare:cloud' v='" << xmlEscape( cloud->getName() ) << "' />\n";
	stream << "    <tag k='source:cloudcompare:axis' v='" << ( alongX ? "x" : "y" ) << "' />\n";
	stream << "    <tag k='source:cloudcompare:origin_lat' v='" << formatNodeAttribute( origin.latitude, 8 ) << "' />\n";
	stream << "    <tag k='source:cloudcompare:origin_lon' v='" << formatNodeAttribute( origin.longitude, 8 ) << "' />\n";
	stream << "    <tag k='source:cloudcompare:origin_alt' v='" << formatNodeAttribute( origin.altitude, 3 ) << "' />\n";
	stream << "    <tag k='source:cloudcompare:frame' v='local_cartesian_enu' />\n";
	stream << "  </relation>\n";
	stream << "</osm>\n";

	if ( !file.commit() )
	{
		errorMessage = QString( "Failed to finalize `%1`." ).arg( outputPath );
		return false;
	}

	return true;
}
} // namespace

Lanelet2MapDemo::Lanelet2MapDemo( QObject* parent )
	: QObject( parent )
	, ccStdPluginInterface( ":/CC/plugin/Lanelet2MapDemo/info.json" )
	, m_action( nullptr )
{
}

void Lanelet2MapDemo::onNewSelection( const ccHObject::Container& selectedEntities )
{
	if ( m_action )
	{
		m_action->setEnabled(
			selectedEntities.size() == 1 && ccHObjectCaster::ToPointCloud( selectedEntities.front() ) != nullptr );
	}
}

QList<QAction*> Lanelet2MapDemo::getActions()
{
	if ( !m_action )
	{
		m_action = new QAction( getName(), this );
		m_action->setToolTip( getDescription() );
		m_action->setIcon( getIcon() );
		connect( m_action, &QAction::triggered, this, &Lanelet2MapDemo::doAction );
	}

	return { m_action };
}

void Lanelet2MapDemo::doAction()
{
	if ( !m_app )
	{
		return;
	}

	const ccHObject::Container& selectedEntities = m_app->getSelectedEntities();
	ccPointCloud* cloud = ( selectedEntities.size() == 1 ? ccHObjectCaster::ToPointCloud( selectedEntities.front() ) : nullptr );
	if ( !cloud )
	{
		m_app->dispToConsole( "Select one point cloud first.", ccMainAppInterface::ERR_CONSOLE_MESSAGE );
		return;
	}

	CCVector3d minCorner;
	CCVector3d maxCorner;
	if ( !cloud->getOwnGlobalBB( minCorner, maxCorner ) )
	{
		m_app->dispToConsole( "The selected point cloud has an invalid bounding box.", ccMainAppInterface::ERR_CONSOLE_MESSAGE );
		return;
	}

	Lanelet2MapDialog dialog( m_app->getMainWindow() );
	dialog.setCloudSummary( buildCloudSummary( cloud, minCorner, maxCorner ) );
	if ( dialog.exec() != QDialog::Accepted )
	{
		return;
	}

	QString errorMessage;
	if ( !writeLaneletMap( dialog.outputPath(), cloud, dialog, errorMessage ) )
	{
		m_app->dispToConsole( errorMessage, ccMainAppInterface::ERR_CONSOLE_MESSAGE );
		QMessageBox::critical( m_app->getMainWindow(), "Lanelet2 Map Demo", errorMessage );
		return;
	}

	m_app->dispToConsole(
		QString( "Lanelet2 demo map exported to `%1`. Reuse the dialog origin with a LocalCartesian projector to preserve the cloud frame." )
			.arg( dialog.outputPath() ),
		ccMainAppInterface::STD_CONSOLE_MESSAGE );
}
