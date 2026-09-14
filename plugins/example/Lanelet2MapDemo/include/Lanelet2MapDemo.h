#pragma once

#include "ccStdPluginInterface.h"

class QAction;
class Lanelet2MapDialog;
class ccPointCloud;

class Lanelet2MapDemo : public QObject
    , public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES(ccPluginInterface ccStdPluginInterface)
	Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.Lanelet2MapDemo" FILE "../info.json")

  public:
	explicit Lanelet2MapDemo(QObject* parent = nullptr);
	~Lanelet2MapDemo() override = default;

	void            onNewSelection(const ccHObject::Container& selectedEntities) override;
	QList<QAction*> getActions() override;

  private:
	void doAction();

	QAction*           m_action;
	Lanelet2MapDialog* m_dialog;
	ccPointCloud*      m_selectedCloud;
};
