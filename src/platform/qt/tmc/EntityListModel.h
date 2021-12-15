#pragma once

#include <QtCore/QAbstractListModel>

namespace QGBA {

struct EntityData {
	uint32_t addr;
	int listId;
	int entryId;
	uint8_t kind;
	QString name;
};

class EntityListModel : public QAbstractListModel {
	Q_OBJECT
public:
	EntityListModel(QObject* parent = nullptr);
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void setEntities(QList<EntityData> entities);
	EntityData getEntity(const QModelIndex& index);

private:
	QList<EntityData> m_entities;
};

}