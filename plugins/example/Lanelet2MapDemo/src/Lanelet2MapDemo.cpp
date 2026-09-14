#include "Lanelet2MapDemo.h"

#include "Lanelet2MapDialog.h"

#include <QAction>
#include <ccHObjectCaster.h>
#include <ccPointCloud.h>

Lanelet2MapDemo::Lanelet2MapDemo(QObject* parent)
    : QObject(parent)
    , ccStdPluginInterface(":/CC/plugin/Lanelet2MapDemo/info.json")
    , m_action(nullptr)
    , m_dialog(nullptr)
    , m_selectedCloud(nullptr)
{
}

void Lanelet2MapDemo::onNewSelection(const ccHObject::Container& selectedEntities)
{
	m_selectedCloud = (selectedEntities.size() == 1 ? ccHObjectCaster::ToPointCloud(selectedEntities.front()) : nullptr);

	if (m_action)
	{
		m_action->setEnabled(true);
	}

	if (m_dialog)
	{
		m_dialog->setCurrentCloud(m_selectedCloud);
	}
}

QList<QAction*> Lanelet2MapDemo::getActions()
{
	if (!m_action)
	{
		m_action = new QAction(getName(), this);
		m_action->setToolTip(getDescription());
		m_action->setIcon(getIcon());
		m_action->setEnabled(true);
		connect(m_action, &QAction::triggered, this, &Lanelet2MapDemo::doAction);
	}

	return {m_action};
}

void Lanelet2MapDemo::doAction()
{
	if (!m_app)
	{
		return;
	}

	if (!m_dialog)
	{
		m_dialog = new Lanelet2MapDialog(m_app, m_app->getMainWindow());
		m_app->registerOverlayDialog(m_dialog, Qt::TopRightCorner);
	}

	m_dialog->setCurrentCloud(m_selectedCloud);

	if (!m_dialog->started())
	{
		if (m_app->getActiveGLWindow())
		{
			m_dialog->linkWith(m_app->getActiveGLWindow());
		}

		if (m_dialog->start())
		{
			m_app->updateOverlayDialogsPlacement();
		}
	}
	else
	{
		m_dialog->raise();
		m_dialog->activateWindow();
	}
}
