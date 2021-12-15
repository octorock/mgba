#pragma once

#include <QAbstractListModel>

namespace QGBA {

struct MemoryWatch {
	uint32_t addr;
	QString name;
	std::string type;
};

class MemoryWatchModel : public QAbstractListModel {
	Q_OBJECT
public:
	MemoryWatchModel(QObject* parent = nullptr);
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	// void setEntities(QList<EntityData> entities);
	void addMemoryWatch(MemoryWatch watch);
	MemoryWatch getMemoryWatch(const QModelIndex& index);

private:
	QList<MemoryWatch> m_watches;
};

}