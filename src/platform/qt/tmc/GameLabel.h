#pragma once
#include <QLabel>

#include "InputController.h"
#include <mgba/core/core.h>

namespace QGBA {
class CoreController;
class GameLabel : public QLabel {
	Q_OBJECT

public:
	GameLabel(QWidget* parent = 0, Qt::WindowFlags f = 0);

	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
    void setController(std::shared_ptr<CoreController> controller);

private:
	std::shared_ptr<CoreController> m_controller = nullptr;
};
}