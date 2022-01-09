/*
 * AutomationEditor.h - declaration of class AutomationEditor which is a window
 *					  where you can edit dynamic values in an easy way
 *
 * Copyright (c) 2006-2008 Javier Serrano Polo <jasp00/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef SAMPLE_EDITOR_H
#define SAMPLE_EDITOR_H

#include <QVector>
#include <QWidget>

#include "Editor.h"

#include "lmms_basics.h"
#include "JournallingObject.h"
#include "TimePos.h"
#include "SampleTCO.h"
#include "ComboBoxModel.h"
#include "Knob.h"

class QPainter;
class QPixmap;
class QScrollBar;

class ComboBox;
class NotePlayHandle;
class TimeLineWidget;



class SampleEditor : public QWidget, public JournallingObject
{
	Q_OBJECT
	Q_PROPERTY(QColor barLineColor MEMBER m_barLineColor)
	Q_PROPERTY(QColor beatLineColor MEMBER m_beatLineColor)
	Q_PROPERTY(QColor lineColor MEMBER m_lineColor)
	Q_PROPERTY(QColor nodeInValueColor MEMBER m_nodeInValueColor)
	Q_PROPERTY(QColor nodeOutValueColor MEMBER m_nodeOutValueColor)
	Q_PROPERTY(QBrush scaleColor MEMBER m_scaleColor)
	Q_PROPERTY(QBrush graphColor MEMBER m_graphColor)
	Q_PROPERTY(QColor crossColor MEMBER m_crossColor)
	Q_PROPERTY(QColor backgroundShade MEMBER m_backgroundShade)
public:
	void setCurrentTCO(SampleTCO * new_tco);

	inline const SampleTCO * currentTCO() const
	{
		return m_tco;
	}

	inline bool validTCO() const
	{
		return m_tco != nullptr;
	}

	void saveSettings(QDomDocument & doc, QDomElement & parent) override;
	void loadSettings(const QDomElement & parent) override;
	QString nodeName() const override
	{
		return "sampleeditor";
	}

	inline void stopRecording()
	{
		m_recording = false;
	}

	inline bool isRecording() const
	{
		return m_recording;
	}


public slots:
	void update();
	void updateAfterTCOChange();


protected:

	void keyPressEvent(QKeyEvent * ke) override;
	void leaveEvent(QEvent * e) override;
	void mousePressEvent(QMouseEvent * mouseEvent) override;
	void mouseDoubleClickEvent(QMouseEvent * mouseEvent) override;
	void mouseReleaseEvent(QMouseEvent * mouseEvent) override;
	void mouseMoveEvent(QMouseEvent * mouseEvent) override;
	void paintEvent(QPaintEvent * pe) override;
	void resizeEvent(QResizeEvent * re) override;
	void wheelEvent(QWheelEvent * we) override;

	float getLevel( int y );
	int xCoordOfTick( int tick );
	int tickOfXCoord( int x );
	float yCoordOfLevel( float level );
	inline void drawLevelTick(QPainter & p, int tick, float value);

	void drawLine( int x0, float y0, int x1, float y1 );

protected slots:
	void play();
	void stop();
	void reverse();
	void record();
	void recordAccompany();
	

	void horScrolled( int new_pos );
	void verScrolled( int new_pos );

	//void setEditMode(AutomationEditor::EditModes mode);
	//void setEditMode(int mode);

	//void setProgressionType(AutomationPattern::ProgressionTypes type);
	//void setProgressionType(int type);
	//void setTension();

	void updatePosition( const TimePos & t );

	void zoomingXChanged();
	void zoomingYChanged();

	/// Updates the pattern's quantization using the current user selected value.
	void setQuantization();

private:

	enum Actions
	{
		NONE,
		SELECT,
		//MOVE,
		KNIFE
	};

	// some constants...
	static const int SCROLLBAR_SIZE = 12;
	static const int TOP_MARGIN = 16;

	static const int DEFAULT_Y_DELTA = 6;
	static const int DEFAULT_STEPS_PER_BAR = 16;
	static const int DEFAULT_PPB = 12 * DEFAULT_STEPS_PER_BAR;

	static const int VALUES_WIDTH = 64;

	SampleEditor();
	SampleEditor( const SampleEditor & );
	virtual ~SampleEditor();

	static QPixmap * s_toolReverse;

	ComboBoxModel m_zoomingXModel;
	ComboBoxModel m_zoomingYModel;
	ComboBoxModel m_quantizeModel;

	static const QVector<float> m_zoomXLevels;

	SampleTCO * m_tco;

	float m_minLevel;
	float m_maxLevel;
	float m_step;
	float m_scrollLevel;
	float m_bottomLevel;
	float m_topLevel;

	void centerTopBottomScroll();
	void updateTopBottomLevels();

	QScrollBar * m_leftRightScroll;
	QScrollBar * m_topBottomScroll;

	TimePos m_currentPosition;
	bool m_recording;

	Actions m_action;

	int m_mouseStartX;
	int m_mouseCurrentX;

	int m_selectionStart;
	int m_selectionEnd;
	bool m_draggingSelection;

	int m_moveXOffset;

	float m_drawLastLevel;
	tick_t m_drawLastTick;

	int m_ppb;
	int m_y_delta;
	bool m_y_auto;

	bool m_mouseDownLeft;
	bool m_mouseDownRight; //true if right click is being held down

	TimeLineWidget * m_timeLine;
	bool m_scrollBack;

	void drawCross(QPainter & p );
	bool inBBEditor();

	QColor m_barLineColor;
	QColor m_beatLineColor;
	QColor m_lineColor;
	QBrush m_graphColor;
	QColor m_nodeInValueColor;
	QColor m_nodeOutValueColor;
	QBrush m_scaleColor;
	QColor m_crossColor;
	QColor m_backgroundShade;

	friend class SampleEditorWindow;


signals:
	void currentPatternChanged();
	void positionChanged( const TimePos & );
	void nameChanged();
} ;




class SampleEditorWindow : public Editor
{
	Q_OBJECT

	static const int INITIAL_WIDTH = 860;
	static const int INITIAL_HEIGHT = 480;
public:
	SampleEditorWindow();
	~SampleEditorWindow();

	void setCurrentTCO(SampleTCO* tco);
	const SampleTCO* currentTCO();

	void dropEvent( QDropEvent * _de ) override;
	void dragEnterEvent( QDragEnterEvent * _dee ) override;

	void open(SampleTCO* SampleTCO);

	SampleEditor* m_editor;

	QSize sizeHint() const override;

public slots:
	void clearCurrentPattern();

signals:
	void currentPatternChanged();

protected:
	void focusInEvent(QFocusEvent * event) override;

protected slots:
	void play() override;
	void stop() override;
	void reverse();
	void record();
	void recordAccompany();
	bool isRecording() const;
	void stopRecording();

private slots:
	void updateWindowTitle();

private:
	QAction* m_reverseAction;

	ComboBox * m_zoomingXComboBox;
	ComboBox * m_zoomingYComboBox;
	ComboBox * m_quantizeComboBox;
};


#endif
