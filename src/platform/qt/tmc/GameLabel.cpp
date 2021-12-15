#include "GameLabel.h"
#include "CoreController.h"
#include <QKeyEvent>

using namespace QGBA;

GameLabel::GameLabel(QWidget * parent, Qt::WindowFlags f )
    : QLabel( parent, f ) {
        
    }

void GameLabel::keyPressEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyPressEvent(event);
		return;
	}
	GBAKey key = m_controller->m_inputController->mapKeyboard(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	if (m_controller) {
		m_controller->addKey(key);
	}
	event->accept();
}

void GameLabel::keyReleaseEvent(QKeyEvent* event) {
	if (event->isAutoRepeat()) {
		QWidget::keyReleaseEvent(event);
		return;
	}
	GBAKey key = m_controller->m_inputController->mapKeyboard(event->key());
	if (key == GBA_KEY_NONE) {
		QWidget::keyPressEvent(event);
		return;
	}
	if (m_controller) {
		m_controller->clearKey(key);
	}
	event->accept();
}

void GameLabel::setController(std::shared_ptr<CoreController> controller) {
    m_controller = controller;
}