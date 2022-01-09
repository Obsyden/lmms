/*
 * SampleEditor.cpp - implementation of SampleEditor which is used for
 *						actual setting of dynamic values
 *
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2008-2013 Paul Giblock <pgib/at/users.sourceforge.net>
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

#include "SampleEditor.h"

#include <cmath>

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMdiArea>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyleOption>
#include <QToolTip>

#include <QDebug>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#include "ActionGroup.h"
#include "AudioDevice.h"
//#include "SampleNode.h"
#include "BBTrackContainer.h"
#include "ComboBox.h"
#include "debug.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "gui_templates.h"
#include "MainWindow.h"
#include "PianoRoll.h"
#include "Note.h"
#include "ProjectJournal.h"
#include "SongEditor.h"
#include "StringPairDrag.h"
#include "TextFloat.h"
#include "TimeLineWidget.h"
#include "ToolTip.h"

#include "SamplePlayHandle.h"

const QVector<float> SampleEditor::m_zoomXLevels =
		{ 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };

QPixmap * SampleEditor::s_toolReverse = nullptr;

SampleEditor::SampleEditor() :
	QWidget(),
	m_zoomingXModel(),
	m_zoomingYModel(),
	m_quantizeModel(),
	m_tco( nullptr ),
	m_minLevel( 0 ),
	m_maxLevel( 0 ),
	m_step( 1 ),
	m_scrollLevel( 0 ),
	m_bottomLevel( 0 ),
	m_topLevel( 0 ),
	m_currentPosition(),
	m_action( NONE ),
	m_drawLastLevel( 0.0f ),
	m_drawLastTick( 0 ),
	m_ppb( DEFAULT_PPB ),
	m_y_delta( DEFAULT_Y_DELTA ),
	m_y_auto( true ),
	m_mouseDownLeft(false),
	m_mouseDownRight( false ),
	m_scrollBack( false ),
	m_barLineColor(0, 0, 0),
	m_beatLineColor(0, 0, 0),
	m_lineColor(0, 0, 0),
	m_recording (false),
	m_graphColor(Qt::SolidPattern),
	m_nodeInValueColor(0, 0, 0),
	m_nodeOutValueColor(0, 0, 0),
	m_scaleColor(Qt::SolidPattern),
	m_crossColor(0, 0, 0),
	m_backgroundShade(0, 0, 0)
{
	connect( this, SIGNAL( currentPatternChanged() ),
				this, SLOT( updateAfterTCOChange() ),
				Qt::QueuedConnection );
	connect( Engine::getSong(), SIGNAL( timeSignatureChanged( int, int ) ),
						this, SLOT( update() ) );

	setAttribute( Qt::WA_OpaquePaintEvent, true );

	//keeps the direction of the widget, undepended on the locale
	setLayoutDirection( Qt::LeftToRight );

	for (auto q : Quantizations) {
		m_quantizeModel.addItem(QString("1/%1").arg(q));
	}

	connect( &m_quantizeModel, SIGNAL(dataChanged() ),
					this, SLOT( setQuantization() ) );
	m_quantizeModel.setValue( m_quantizeModel.findText( "1/8" ) );

	// add time-line
	m_timeLine = new TimeLineWidget( VALUES_WIDTH, 0, m_ppb,
				Engine::getSong()->getPlayPos(
					Song::Mode_PlaySample ),
					m_currentPosition,
					Song::Mode_PlaySample, this );
	connect( this, SIGNAL( positionChanged( const TimePos & ) ),
		m_timeLine, SLOT( updatePosition( const TimePos & ) ) );
	connect( m_timeLine, SIGNAL( positionChanged( const TimePos & ) ),
			this, SLOT( updatePosition( const TimePos & ) ) );

	// init scrollbars
	m_leftRightScroll = new QScrollBar( Qt::Horizontal, this );
	m_leftRightScroll->setSingleStep( 1 );
	connect( m_leftRightScroll, SIGNAL( valueChanged( int ) ), this,
						SLOT( horScrolled( int ) ) );

	m_topBottomScroll = new QScrollBar( Qt::Vertical, this );
	m_topBottomScroll->setSingleStep( 1 );
	m_topBottomScroll->setPageStep( 20 );
	connect( m_topBottomScroll, SIGNAL( valueChanged( int ) ), this,
						SLOT( verScrolled( int ) ) );

	// init pixmaps
	if (s_toolReverse == nullptr)
	{
		s_toolReverse = new QPixmap(embed::getIconPixmap("flip_x"));
	}

	setCurrentTCO(nullptr);

	setMouseTracking( true );
	setFocusPolicy( Qt::StrongFocus );
	setFocus();
}




SampleEditor::~SampleEditor()
{
	m_zoomingXModel.disconnect();
	m_zoomingYModel.disconnect();
	m_quantizeModel.disconnect();
}




void SampleEditor::setCurrentTCO(SampleTCO * new_tco )
{
	if (m_tco)
	{
		m_tco->disconnect(this);
	}

	m_tco = new_tco;

	if (m_tco != nullptr)
	{
		connect(m_tco, SIGNAL(dataChanged()), this, SLOT(update()));
		connect(m_tco, SIGNAL(sampleChanged()), this, SLOT(update()));
	}

	emit currentPatternChanged();
}




void SampleEditor::saveSettings(QDomDocument & doc, QDomElement & dom_parent)
{
	MainWindow::saveWidgetState( parentWidget(), dom_parent );
}




void SampleEditor::loadSettings( const QDomElement & dom_parent)
{
	MainWindow::restoreWidgetState(parentWidget(), dom_parent);
}




void SampleEditor::updateAfterTCOChange()
{
	m_currentPosition = 0;

	if( !validTCO() )
	{
		m_minLevel = m_maxLevel = m_scrollLevel = 0;
		m_step = 1;
		resizeEvent(nullptr);
		return;
	}

	//m_minLevel = m_tco->firstObject()->minValue<float>();
	//m_maxLevel = m_pattern->firstObject()->maxValue<float>();
	//m_step = m_pattern->firstObject()->step<float>();
	centerTopBottomScroll();

	// resizeEvent() does the rest for us (scrolling, range-checking
	// of levels and so on...)
	resizeEvent(nullptr);

	update();
}




void SampleEditor::update()
{
	QWidget::update();
}




void SampleEditor::keyPressEvent(QKeyEvent * ke )
{
	switch( ke->key() )
	{
		case Qt::Key_Up:
			m_topBottomScroll->setValue(
					m_topBottomScroll->value() - 1 );
			ke->accept();
			break;
		
		case Qt::Key_Delete:
			if(m_selectionEnd>m_tco->length() && m_selectionStart<m_tco->length())
				m_selectionEnd=m_tco->length();
			if (m_selectionEnd - m_selectionStart > 0 && m_selectionEnd <= m_tco->length())
			{
				m_tco->getTrack()->addJournalCheckPoint();
				m_tco->getTrack()->saveJournallingState( false );
				auto framesPerTick = Engine::framesPerTick ( m_tco->sampleBuffer ()->sampleRate () );
				m_tco->sampleBuffer()->removeSection(m_selectionStart*framesPerTick, m_selectionEnd*framesPerTick);
				m_tco->updateTrackTcos();
				m_tco->changeLength(m_tco->length()-m_selectionEnd+m_selectionStart);
				m_tco->getTrack()->restoreJournallingState();
				repaint();
			}
				

		case Qt::Key_Down:
			m_topBottomScroll->setValue(
					m_topBottomScroll->value() + 1 );
			ke->accept();
			break;

		case Qt::Key_Left:
			if( ( m_timeLine->pos() -= 16 ) < 0 )
			{
				m_timeLine->pos().setTicks( 0 );
			}
			m_timeLine->updatePosition();
			ke->accept();
			break;

		case Qt::Key_Right:
			m_timeLine->pos() += 16;
			m_timeLine->updatePosition();
			ke->accept();
			break;

		case Qt::Key_Home:
			m_timeLine->pos().setTicks( 0 );
			m_timeLine->updatePosition();
			ke->accept();
			break;

		default:
			break;
	}
}




void SampleEditor::leaveEvent(QEvent * e )
{
	while (QApplication::overrideCursor() != nullptr)
	{
		QApplication::restoreOverrideCursor();
	}
	QWidget::leaveEvent( e );
	update();
}


void SampleEditor::drawLine( int x0In, float y0, int x1In, float y1 )
{
	int x0 = Note::quantized( x0In, 1 );
	int x1 = Note::quantized( x1In, 1 );
	int deltax = qAbs( x1 - x0 );
	float deltay = qAbs<float>( y1 - y0 );
	int x = x0;
	float y = y0;
	int xstep;
	int ystep;

	if( deltax < 1 )
	{
		return;
	}

	deltax /= 1;

	float yscale = deltay / ( deltax );

	if( x0 < x1 )
	{
		xstep = 1;
	}
	else
	{
		xstep = -( 1 );
	}

	float lineAdjust;
	if( y0 < y1 )
	{
		ystep = 1;
		lineAdjust = yscale;
	}
	else
	{
		ystep = -1;
		lineAdjust = -( yscale );
	}

	int i = 0;
	while( i < deltax )
	{
		y = y0 + ( ystep * yscale * i ) + lineAdjust;

		x += xstep;
		i += 1;
	}
}






void SampleEditor::mouseDoubleClickEvent(QMouseEvent * mouseEvent)
{
	QString af = m_tco->m_sampleBuffer->openAudioFile();

	if ( af.isEmpty() ) {} //Don't do anything if no file is loaded
	else if ( af == m_tco->m_sampleBuffer->audioFile() )
	{	//Instead of reloading the existing file, just reset the size
		int length = (int) ( m_tco->m_sampleBuffer->frames() / Engine::framesPerTick() );
		m_tco->changeLength(length);
		update();
	}
	else
	{	//Otherwise load the new file as ususal
		m_tco->setSampleFile( af );
		Engine::getSong()->setModified();
		updateAfterTCOChange();
		m_tco->updateTrackTcos();
		emit nameChanged();
		update();
	}
}

void SampleEditor::mousePressEvent(QMouseEvent * mouseEvent)
{
	//TODO: Implement
	m_mouseStartX = mouseEvent->x();
	int startTick = tickOfXCoord(m_mouseStartX);
	if (mouseEvent->button() == Qt::LeftButton)
		m_mouseDownLeft = true;
	if (mouseEvent->button() == Qt::RightButton)
	{
		m_mouseDownRight = true;

		//Knife

		const TimePos splitPos = startTick;
		//Don't split if we slid off the TCO or if we're on the clip's start/end
		//Cutting at exactly the start/end position would create a zero length
		//clip (bad), and a clip the same length as the original one (pointless).
		if ( splitPos > 0 && splitPos < m_tco->length() )
		{
			m_tco->getTrack()->addJournalCheckPoint();
			m_tco->getTrack()->saveJournallingState( false );

			SampleTCO * rightTCO = new SampleTCO ( *m_tco );
			auto framesPerTick = Engine::framesPerTick ( m_tco->sampleBuffer ()->sampleRate () );
			rightTCO->movePosition( splitPos );

			rightTCO->sampleBuffer()->trim(framesPerTick*splitPos, m_tco->sampleBuffer()->frames());
			rightTCO->changeLength(m_tco->length()-splitPos);
			emit rightTCO->dataChanged();

			m_tco->sampleBuffer()->trim(0, framesPerTick*splitPos);
			m_tco->changeLength(splitPos);
			m_tco->updateTrackTcos();
			emit m_tco->dataChanged();
			m_tco->getTrack()->restoreJournallingState();
		}
	}
		

	//if ( startTick > m_selectionStart && startTick < m_selectionEnd )
	//	m_action = MOVE;
	
	if ( m_action == NONE )
	{
		m_action = SELECT;
		m_selectionStart = startTick;
		f_cnt_t frameStart = m_selectionStart*Engine::framesPerTick();
		m_selectionEnd = startTick;
	}
}



void SampleEditor::mouseReleaseEvent(QMouseEvent * mouseEvent )
{
	bool mustRepaint = false;

	if (mouseEvent->button() == Qt::LeftButton)
	{
		m_mouseDownLeft = false;
		mustRepaint = true;
	}
	if (mouseEvent->button() == Qt::RightButton)
	{
		m_mouseDownRight = false;
		mustRepaint = true;
	}

	/*
	if ( m_action == MOVE )
	{
		if(m_selectionEnd>m_selectionStart && m_selectionStart>0)
		{
			//Sample buffer move frames function
			auto framesPerTick = Engine::framesPerTick ( m_tco->sampleBuffer ()->sampleRate () );
			m_tco->getTrack()->addJournalCheckPoint();
			m_tco->getTrack()->saveJournallingState( false );
			m_tco->sampleBuffer()->moveSection(m_selectionStart*framesPerTick, m_selectionEnd*framesPerTick,
			framesPerTick*tickOfXCoord(-m_mouseStartX+m_mouseCurrentX));
			m_tco->changeLength(m_tco->sampleBuffer()->frames()/framesPerTick);
			emit m_tco->dataChanged();
			m_tco->updateTrackTcos();
			m_tco->getTrack()->restoreJournallingState();
			update();
		}
	}
	*/

	m_action = NONE;

	if (mustRepaint) { repaint(); }
}

int SampleEditor::tickOfXCoord( int x ){
	return (x - VALUES_WIDTH * TimePos::ticksPerBar() / m_ppb) + m_currentPosition;
}


void SampleEditor::mouseMoveEvent(QMouseEvent * mouseEvent )
{
	if( !validTCO() )
	{
		update();
		return;
	}

	

	QCursor select(Qt::IBeamCursor);
	QCursor move(Qt::OpenHandCursor);
	QCursor defaultC(Qt::ArrowCursor);
	// If the mouse y position is inside the Sample Editor viewport
	if (mouseEvent->y() > TOP_MARGIN)
	{
		float level = getLevel(mouseEvent->y());
		// Get the viewport X position where the mouse is at
		int x = mouseEvent->x();

		m_mouseCurrentX = x;

		// Get the X position in ticks
		int posTicks = tickOfXCoord(x);

		switch( m_action )
		{
			case SELECT:
				QApplication::setOverrideCursor(select);
				if ( m_mouseDownLeft )
				{
					//If the cursor if behind selection start
					if ( x > m_mouseStartX )
						m_selectionEnd = posTicks;
					else
						m_selectionStart = posTicks;
					
				}
				break;
			/*
			case MOVE:
				QApplication::setOverrideCursor(move);
				if ( m_mouseDownLeft )
				{
					if ( m_selectionStart < m_selectionEnd )
					{
						m_draggingSelection=true;
					}
				}
				break;
			*/
			case NONE:
				/*
				if ( x < xCoordOfTick(m_selectionEnd) && x > xCoordOfTick(m_selectionStart) )
				{
					QApplication::setOverrideCursor(move);
				}
					
				else
				{
					*/
					QApplication::setOverrideCursor(defaultC);
				//}
					
				break;

			default:
				break;
		}
	}
	else // If the mouse Y position is above the SampleEditor viewport
	{
		QApplication::restoreOverrideCursor();
	}

	update();
}




inline void SampleEditor::drawCross( QPainter & p )
{
	QPoint mouse_pos = mapFromGlobal( QCursor::pos() );
	int grid_bottom = height() - SCROLLBAR_SIZE - 1;
	float level = getLevel( mouse_pos.y() );
	float cross_y = m_y_auto ?
		grid_bottom - ( ( grid_bottom - TOP_MARGIN )
				* ( level - m_minLevel )
				/ (float)( m_maxLevel - m_minLevel ) ) :
		grid_bottom - ( level - m_bottomLevel ) * m_y_delta;

	p.setPen(m_crossColor);
	p.drawLine( VALUES_WIDTH, (int) cross_y, width(), (int) cross_y );
	p.drawLine( mouse_pos.x(), TOP_MARGIN, mouse_pos.x(), height() - SCROLLBAR_SIZE );


	QPoint tt_pos =  QCursor::pos();
	tt_pos.ry() -= 51;
	tt_pos.rx() += 26;

	//float scaledLevel = m_pattern->firstObject()->scaledValue( level );

	// Limit the scaled-level tooltip to the grid
	if( mouse_pos.x() >= 0 &&
		mouse_pos.x() <= width() - SCROLLBAR_SIZE &&
		mouse_pos.y() >= 0 &&
		mouse_pos.y() <= height() - SCROLLBAR_SIZE )
	{
		//QToolTip::showText( tt_pos, QString::number( scaledLevel ), this );
	}
}


void SampleEditor::paintEvent(QPaintEvent * pe )
{
	QStyleOption opt;
	opt.initFrom( this );
	QPainter p( this );
	style()->drawPrimitive( QStyle::PE_Widget, &opt, &p, this );

	// get foreground color
	QColor fgColor = p.pen().brush().color();
	// get background color and fill background
	QBrush bgColor = p.background();
	p.fillRect( 0, 0, width(), height(), bgColor );

	// set font-size to 8
	p.setFont( pointSize<8>( p.font() ) );

	int grid_height = height() - TOP_MARGIN - SCROLLBAR_SIZE;

	// start drawing at the bottom
	int grid_bottom = height() - SCROLLBAR_SIZE - 1;

	//p.fillRect(0, TOP_MARGIN, VALUES_WIDTH, height() - TOP_MARGIN, m_scaleColor);

	// print value numbers
	int font_height = p.fontMetrics().height();
	Qt::Alignment text_flags =
		(Qt::Alignment)( Qt::AlignRight | Qt::AlignVCenter );


	// set clipping area, because we are not allowed to paint over
	// keyboard...
	p.setClipRect( VALUES_WIDTH, TOP_MARGIN, width() - VALUES_WIDTH,
								grid_height  );


	// draw vertical raster

	if( validTCO() )
	{
		//Vertical lines
		int tick, x, q;
		int x_line_end = (int)( m_y_auto || m_topLevel < m_maxLevel ?
			TOP_MARGIN :
			grid_bottom - ( m_topLevel - m_bottomLevel ) * m_y_delta );
		q = DefaultTicksPerBar / Quantizations[m_quantizeModel.value()];
		for( tick = m_currentPosition - m_currentPosition % q,
				 x = xCoordOfTick( tick );
			 x<=width();
			 tick += q, x = xCoordOfTick( tick ) )
		{
			p.setPen(m_lineColor);
			p.drawLine( x, grid_bottom, x, x_line_end );
		}

		//Horizontal lines
		if( m_y_auto )
		{
			QPen pen(m_beatLineColor);
			pen.setStyle( Qt::DotLine );
			p.setPen( pen );
			float y_delta = ( grid_bottom - TOP_MARGIN ) / 8.0f;
			for( int i = 1; i < 8; ++i )
			{
				int y = (int)( grid_bottom - i * y_delta );
				p.drawLine( VALUES_WIDTH, y, width(), y );
			}
		}
		else
		{
			float y;
			for( int level = (int)m_bottomLevel; level <= m_topLevel; level++)
			{
				y =  yCoordOfLevel( (float)level );

				p.setPen(level % 10 == 0 ? m_beatLineColor : m_lineColor);

				// draw level line
				p.drawLine( VALUES_WIDTH, (int) y, width(), (int) y );
			}
		}
		
		GuiApplication::instance()->pianoRoll()->m_editor->paintGhostNotes(p, rect(), m_currentPosition, m_currentPosition);

		//draw sample waveform
		if(m_tco->sampleBuffer()->sampleLength()>0)
		{
			auto bufferFramesPerTick = Engine::framesPerTick ( m_tco->sampleBuffer ()->sampleRate () );
			TimePos tp = TimePos(m_leftRightScroll->value());
			p.setPen(fgColor);
			m_tco->sampleBuffer()->visualize(p, 
			QRect(VALUES_WIDTH,0, 
			xCoordOfTick(m_tco->sampleLength().getTicks())-VALUES_WIDTH, height() - (SCROLLBAR_SIZE + TOP_MARGIN)),
			tp.getTicks()*bufferFramesPerTick,
			m_tco->sampleBuffer()->frames());
			//draw floating waveform selection
			/*
			if(m_action==MOVE)
			{
				m_tco->sampleBuffer()->visualize(p, 
				QRect(xCoordOfTick(m_selectionStart)+(-m_mouseStartX+m_mouseCurrentX), 0, 
				xCoordOfTick(m_selectionEnd)-xCoordOfTick(m_selectionStart), height() - (SCROLLBAR_SIZE + TOP_MARGIN)),
				m_selectionStart*bufferFramesPerTick,
				m_selectionEnd*bufferFramesPerTick);
			}
			*/
		}
		QColor selectionColor = bgColor.color().lighter(200); selectionColor.setAlpha(100);
		//Draw selection
		if (m_selectionEnd>m_selectionStart)
		{
			p.fillRect(
			QRect(xCoordOfTick(m_selectionStart), 0, xCoordOfTick(m_selectionEnd)-xCoordOfTick(m_selectionStart), height() - (SCROLLBAR_SIZE + TOP_MARGIN)),
			selectionColor);
		}

	}



	// following code draws all visible values
	
	else{
		QFont f = p.font();
		f.setBold( true );
		p.setFont( pointSize<14>( f ) );
		p.setPen( QApplication::palette().color( QPalette::Active,
							QPalette::BrightText ) );
		p.drawText( VALUES_WIDTH + 20, TOP_MARGIN + 40,
				width() - VALUES_WIDTH - 20 - SCROLLBAR_SIZE,
				grid_height - 40, Qt::TextWordWrap,
				tr( "Please open an Sample with "
					"the context menu of a control!" ) );
	}

	// TODO: Get this out of paint event
	int l = validTCO() ? (int) m_tco->length() : 0;

	// reset scroll-range
	if( m_leftRightScroll->maximum() != l )
	{
		m_leftRightScroll->setRange( 0, l );
		m_leftRightScroll->setPageStep( l );
	}

	if(validTCO() && GuiApplication::instance()->sampleEditor()->m_editor->hasFocus())
	{
		drawCross( p );
	}

	const QPixmap * cursor = nullptr;
	// draw current edit-mode-icon below the cursor
	QPoint mousePosition = mapFromGlobal( QCursor::pos() );
	if (cursor != nullptr && mousePosition.y() > TOP_MARGIN + SCROLLBAR_SIZE)
	{
		p.drawPixmap( mousePosition + QPoint( 8, 8 ), *cursor );
	}
}




int SampleEditor::xCoordOfTick(int tick )
{
	return VALUES_WIDTH + ( ( tick - m_currentPosition )
		* m_ppb / TimePos::ticksPerBar() );
}




float SampleEditor::yCoordOfLevel(float level )
{
	int grid_bottom = height() - SCROLLBAR_SIZE - 1;
	if( m_y_auto )
	{
		return ( grid_bottom - ( grid_bottom - TOP_MARGIN ) * ( level - m_minLevel ) / ( m_maxLevel - m_minLevel ) );
	}
	else
	{
		return ( grid_bottom - ( level - m_bottomLevel ) * m_y_delta );
	}
}




void SampleEditor::drawLevelTick(QPainter & p, int tick, float value)
{
	int grid_bottom = height() - SCROLLBAR_SIZE - 1;
	const int x = xCoordOfTick( tick );
	int rect_width = xCoordOfTick( tick+1 ) - x;

	// is the level in visible area?
	if( ( value >= m_bottomLevel && value <= m_topLevel )
			|| ( value > m_topLevel && m_topLevel >= 0 )
			|| ( value < m_bottomLevel && m_bottomLevel <= 0 ) )
	{
		int y_start = yCoordOfLevel( value );
		int rect_height;

		if( m_y_auto )
		{
			int y_end = (int)( grid_bottom
						+ ( grid_bottom - TOP_MARGIN )
						* m_minLevel
						/ ( m_maxLevel - m_minLevel ) );

			rect_height = y_end - y_start;
		}
		else
		{
			rect_height = (int)( value * m_y_delta );
		}

		QBrush currentColor = m_graphColor;

		p.fillRect( x, y_start, rect_width, rect_height, currentColor );
	}
#ifdef LMMS_DEBUG
	else
	{
		printf("not in range\n");
	}
#endif
}




// Center the vertical scroll position on the first object's inValue
void SampleEditor::centerTopBottomScroll()
{
	// default to the m_scrollLevel position
	int pos = static_cast<int>(m_scrollLevel);
	// If a pattern exists...
	if (m_tco)
	{
		
	}
	m_topBottomScroll->setValue(pos);
}




// responsible for moving/resizing scrollbars after window-resizing
void SampleEditor::resizeEvent(QResizeEvent * re)
{
	m_leftRightScroll->setGeometry( VALUES_WIDTH, height() - SCROLLBAR_SIZE,
							width() - VALUES_WIDTH,
							SCROLLBAR_SIZE );

	int grid_height = height() - TOP_MARGIN - SCROLLBAR_SIZE;
	m_topBottomScroll->setGeometry( width() - SCROLLBAR_SIZE, TOP_MARGIN,
						SCROLLBAR_SIZE, grid_height );

	int half_grid = grid_height / 2;
	int total_pixels = (int)( ( m_maxLevel - m_minLevel ) * m_y_delta + 1 );
	if( !m_y_auto && grid_height < total_pixels )
	{
		int min_scroll = (int)( m_minLevel + floorf( half_grid
							/ (float)m_y_delta ) );
		int max_scroll = (int)( m_maxLevel - (int)floorf( ( grid_height
					- half_grid ) / (float)m_y_delta ) );
		m_topBottomScroll->setRange( min_scroll, max_scroll );
	}
	else
	{
		m_topBottomScroll->setRange( (int) m_scrollLevel,
							(int) m_scrollLevel );
	}
	centerTopBottomScroll();

	if( Engine::getSong() )
	{
		Engine::getSong()->getPlayPos( Song::Mode_PlaySample
					).m_timeLine->setFixedWidth( width() );
	}

	updateTopBottomLevels();
	update();
}




// TODO: Move this method up so it's closer to the other mouse events
void SampleEditor::wheelEvent(QWheelEvent * we )
{
	we->accept();
	if( we->modifiers() & Qt::ControlModifier && we->modifiers() & Qt::ShiftModifier )
	{
		int y = m_zoomingYModel.value();
		if(we->angleDelta().y() > 0)
		{
			y++;
		}
		else if(we->angleDelta().y() < 0)
		{
			y--;
		}
		y = qBound( 0, y, m_zoomingYModel.size() - 1 );
		m_zoomingYModel.setValue( y );
	}
	else if( we->modifiers() & Qt::ControlModifier && we->modifiers() & Qt::AltModifier )
	{
		int q = m_quantizeModel.value();
		if((we->angleDelta().x() + we->angleDelta().y()) > 0) // alt + scroll becomes horizontal scroll on KDE
		{
			q--;
		}
		else if((we->angleDelta().x() + we->angleDelta().y()) < 0) // alt + scroll becomes horizontal scroll on KDE
		{
			q++;
		}
		q = qBound( 0, q, m_quantizeModel.size() - 1 );
		m_quantizeModel.setValue( q );
		update();
	}
	else if( we->modifiers() & Qt::ControlModifier )
	{
		int x = m_zoomingXModel.value();
		if(we->angleDelta().y() > 0)
		{
			x++;
		}
		else if(we->angleDelta().y() < 0)
		{
			x--;
		}
		x = qBound( 0, x, m_zoomingXModel.size() - 1 );

		int mouseX = (position( we ).x() - VALUES_WIDTH)* TimePos::ticksPerBar();
		// ticks based on the mouse x-position where the scroll wheel was used
		int ticks = mouseX / m_ppb;
		// what would be the ticks in the new zoom level on the very same mouse x
		int newTicks = mouseX / (DEFAULT_PPB * m_zoomXLevels[x]);

		// scroll so the tick "selected" by the mouse x doesn't move on the screen
		m_leftRightScroll->setValue(m_leftRightScroll->value() + ticks - newTicks);


		m_zoomingXModel.setValue( x );
	}

	// FIXME: Reconsider if determining orientation is necessary in Qt6.
	else if(abs(we->angleDelta().x()) > abs(we->angleDelta().y())) // scrolling is horizontal
	{
		m_leftRightScroll->setValue(m_leftRightScroll->value() -
							we->angleDelta().x() * 2 / 15);
	}
	else if(we->modifiers() & Qt::ShiftModifier)
	{
		m_leftRightScroll->setValue(m_leftRightScroll->value() -
							we->angleDelta().y() * 2 / 15);
	}
	else
	{
		m_topBottomScroll->setValue(m_topBottomScroll->value() -
							(we->angleDelta().x() + we->angleDelta().y()) / 30);
	}
}




float SampleEditor::getLevel(int y )
{
	int level_line_y = height() - SCROLLBAR_SIZE - 1;
	// pressed level
	float level = roundf( ( m_bottomLevel + ( m_y_auto ?
			( m_maxLevel - m_minLevel ) * ( level_line_y - y )
					/ (float)( level_line_y - ( TOP_MARGIN + 2 ) ) :
			( level_line_y - y ) / (float)m_y_delta ) ) / m_step ) * m_step;
	// some range-checking-stuff
	level = qBound( m_bottomLevel, level, m_topLevel );

	return( level );
}




inline bool SampleEditor::inBBEditor()
{
	return( validTCO() &&
				m_tco->getTrack()->trackContainer() == Engine::getBBTrackContainer() );
}




void SampleEditor::play()
{
	if( ! validTCO() )
	{
		return;
	}

	//if( Engine::getSong()->playMode() != Song::Mode_PlaySample )
	if (Engine::getSong()->isStopped())
	{
		Engine::getSong()->playSample(m_tco);
	}
	else
	{
		Engine::getSong()->togglePause();
	}
}




void SampleEditor::stop()
{
	if( !validTCO() )
	{
		return;
	}
	if( m_tco->getTrack() && inBBEditor() )
	{
		Engine::getBBTrackContainer()->stop();
	}
	else
	{
		Engine::getSong()->stop();
	}
	m_recording = false;
	m_tco->setRecord(false);
	m_scrollBack = true;
}




void SampleEditor::horScrolled(int new_pos )
{
	m_currentPosition = new_pos;
	emit positionChanged( m_currentPosition );
	update();
}

void SampleEditor::reverse()
{
	m_tco->sampleBuffer()->setReversed(!m_tco->sampleBuffer()->reversed());
	m_tco->updateTrackTcos();
	update();
	emit m_tco->wasReversed();
}



void SampleEditor::verScrolled(int new_pos )
{
	m_scrollLevel = new_pos;
	updateTopBottomLevels();
	update();
}






void SampleEditor::updatePosition(const TimePos & t )
{
	if( ( Engine::getSong()->isPlaying() &&
			Engine::getSong()->playMode() ==
					Song::Mode_PlaySample ) ||
							m_scrollBack == true )
	{
		const int w = width() - VALUES_WIDTH;
		if( t > m_currentPosition + w * TimePos::ticksPerBar() / m_ppb )
		{
			m_leftRightScroll->setValue( t.getBar() *
							TimePos::ticksPerBar() );
		}
		else if( t < m_currentPosition )
		{
			TimePos t_ = qMax( t - w * TimePos::ticksPerBar() *
					TimePos::ticksPerBar() / m_ppb, 0 );
			m_leftRightScroll->setValue( t_.getBar() *
							TimePos::ticksPerBar() );
		}
		m_scrollBack = false;
	}
}



void SampleEditor::zoomingXChanged()
{
	m_ppb = m_zoomXLevels[m_zoomingXModel.value()] * DEFAULT_PPB;

	assert( m_ppb > 0 );

	m_timeLine->setPixelsPerBar( m_ppb );
	update();
}




void SampleEditor::zoomingYChanged()
{
	const QString & zfac = m_zoomingYModel.currentText();
	m_y_auto = zfac == "Auto";
	if( !m_y_auto )
	{
		m_y_delta = zfac.left( zfac.length() - 1 ).toInt()
							* DEFAULT_Y_DELTA / 100;
	}
#ifdef LMMS_DEBUG
	assert( m_y_delta > 0 );
#endif
	resizeEvent(nullptr);
}




void SampleEditor::setQuantization()
{

	update();
}




void SampleEditor::updateTopBottomLevels()
{
	if( m_y_auto )
	{
		m_bottomLevel = m_minLevel;
		m_topLevel = m_maxLevel;
		return;
	}

	int total_pixels = (int)( ( m_maxLevel - m_minLevel ) * m_y_delta + 1 );
	int grid_height = height() - TOP_MARGIN - SCROLLBAR_SIZE;
	int half_grid = grid_height / 2;

	if( total_pixels > grid_height )
	{
		int centralLevel = (int)( m_minLevel + m_maxLevel - m_scrollLevel );

		m_bottomLevel = centralLevel - ( half_grid
							/ (float)m_y_delta );
		if( m_bottomLevel < m_minLevel )
		{
			m_bottomLevel = m_minLevel;
			m_topLevel = m_minLevel + (int)floorf( grid_height
							/ (float)m_y_delta );
		}
		else
		{
			m_topLevel = m_bottomLevel + (int)floorf( grid_height
							/ (float)m_y_delta );
			if( m_topLevel > m_maxLevel )
			{
				m_topLevel = m_maxLevel;
				m_bottomLevel = m_maxLevel - (int)floorf(
					grid_height / (float)m_y_delta );
			}
		}
	}
	else
	{
		m_bottomLevel = m_minLevel;
		m_topLevel = m_maxLevel;
	}
}



void SampleEditor::record()
{
	if( Engine::getSong()->isPlaying() )
	{
		stop();
	}
	if( m_recording || ! validTCO() )
	{
		return;
	}

	m_tco->addJournalCheckPoint();
	m_tco->setRecord(true);
	m_recording = true;
	Engine::getSong()->record();
	Engine::getSong()->playSample( m_tco, false );
}




void SampleEditor::recordAccompany()
{
	if( Engine::getSong()->isPlaying() )
	{
		stop();
	}
	if( m_recording || ! validTCO() )
	{
		return;
	}

	m_tco->addJournalCheckPoint();
	m_tco->setRecord(true);
	m_recording = true;
	Engine::getSong()->record();
	if( m_tco->getTrack()->trackContainer() == Engine::getSong() )
	{
		Engine::getSong()->playSong();
	}
	else
	{
		Engine::getSong()->playBB();
	}
}




SampleEditorWindow::SampleEditorWindow() :
	Editor(true),
	m_editor(new SampleEditor())
{
	setCentralWidget(m_editor);

	m_playAction->setToolTip(tr( "Play/pause current sample (Space)" ));
	m_recordAction->setToolTip( tr("Record audio from input device"));
	m_recordAccompanyAction->setToolTip( tr( "Record audio from input device while playing song or BB track" ) );
	if ( !Engine::audioEngine()->audioDev()->supportsCapture() )
	{
		m_toolBar->widgetForAction(m_recordAction)->setDisabled(true);
		m_toolBar->widgetForAction(m_recordAccompanyAction)->setDisabled(true);
	}
	m_stopAction->setToolTip( tr( "Stop playing of current pattern (Space)" ) );

	
	

	m_reverseAction = new QAction(embed::getIconPixmap("flip_x"), tr("Reverse sample"), this);
	m_toolBar->addAction(m_reverseAction);
	m_toolBar->widgetForAction(m_reverseAction)->setObjectName("reverseButton");
	connect(m_reverseAction, SIGNAL(triggered()), this, SLOT(reverse()));


	addToolBarBreak();
	
	// Zoom controls
	DropToolBar *zoomToolBar = addDropToolBarToTop(tr("Zoom controls"));

	QLabel * zoom_x_label = new QLabel( zoomToolBar );
	zoom_x_label->setPixmap( embed::getIconPixmap( "zoom_x" ) );

	m_zoomingXComboBox = new ComboBox( zoomToolBar );
	m_zoomingXComboBox->setFixedSize( 80, ComboBox::DEFAULT_HEIGHT );
	m_zoomingXComboBox->setToolTip( tr( "Horizontal zooming" ) );

	for( float const & zoomLevel : m_editor->m_zoomXLevels )
	{
		m_editor->m_zoomingXModel.addItem( QString( "%1\%" ).arg( zoomLevel * 100 ) );
	}
	m_editor->m_zoomingXModel.setValue( m_editor->m_zoomingXModel.findText( "100%" ) );

	m_zoomingXComboBox->setModel( &m_editor->m_zoomingXModel );

	connect( &m_editor->m_zoomingXModel, SIGNAL( dataChanged() ),
			m_editor, SLOT( zoomingXChanged() ) );


	QLabel * zoom_y_label = new QLabel( zoomToolBar );
	zoom_y_label->setPixmap( embed::getIconPixmap( "zoom_y" ) );

	m_zoomingYComboBox = new ComboBox( zoomToolBar );
	m_zoomingYComboBox->setFixedSize( 80, ComboBox::DEFAULT_HEIGHT );
	m_zoomingYComboBox->setToolTip( tr( "Vertical zooming" ) );

	m_editor->m_zoomingYModel.addItem( "Auto" );
	for( int i = 0; i < 7; ++i )
	{
		m_editor->m_zoomingYModel.addItem( QString::number( 25 << i ) + "%" );
	}
	m_editor->m_zoomingYModel.setValue( m_editor->m_zoomingYModel.findText( "Auto" ) );

	m_zoomingYComboBox->setModel( &m_editor->m_zoomingYModel );

	connect( &m_editor->m_zoomingYModel, SIGNAL( dataChanged() ),
			m_editor, SLOT( zoomingYChanged() ) );

	zoomToolBar->addWidget( zoom_x_label );
	zoomToolBar->addWidget( m_zoomingXComboBox );
	zoomToolBar->addSeparator();
	zoomToolBar->addWidget( zoom_y_label );
	zoomToolBar->addWidget( m_zoomingYComboBox );

	// Quantization controls
	DropToolBar *quantizationActionsToolBar = addDropToolBarToTop(tr("Quantization controls"));

	QLabel * quantize_lbl = new QLabel( m_toolBar );
	quantize_lbl->setPixmap( embed::getIconPixmap( "quantize" ) );

	m_quantizeComboBox = new ComboBox( m_toolBar );
	m_quantizeComboBox->setFixedSize( 60, ComboBox::DEFAULT_HEIGHT );
	m_quantizeComboBox->setToolTip( tr( "Quantization" ) );

	m_quantizeComboBox->setModel( &m_editor->m_quantizeModel );

	quantizationActionsToolBar->addWidget( quantize_lbl );
	quantizationActionsToolBar->addWidget( m_quantizeComboBox );

	connect( m_editor, SIGNAL( nameChanged() ),
			this, SLOT( updateWindowTitle() ) );

	// Setup our actual window
	setFocusPolicy( Qt::StrongFocus );
	setFocus();
	setWindowIcon( embed::getIconPixmap("sample_track") );
	setAcceptDrops( true );
	m_toolBar->setAcceptDrops( true );
}


SampleEditorWindow::~SampleEditorWindow()
{
}


void SampleEditorWindow::setCurrentTCO(SampleTCO* tco)
{
	// Disconnect our old pattern
	if (currentTCO() != nullptr)
	{
		m_editor->m_tco->disconnect(this);
		m_reverseAction->disconnect();
		//m_flipYAction->disconnect();
	}

	m_editor->setCurrentTCO(tco);

	// Set our window's title
	if (tco == nullptr)
	{
		setWindowTitle( tr( "Sample Editor - no pattern" ) );
		return;
	}

	setWindowTitle( tr( "Sample Editor - %1" ).arg( m_editor->m_tco->name() ) );


	// Connect new pattern
	if (tco)
	{
		connect(tco, SIGNAL(dataChanged()), this, SLOT(update()));
		connect( tco, SIGNAL( dataChanged() ), this, SLOT( updateWindowTitle() ) );
		connect(tco, SIGNAL(destroyed()), this, SLOT(clearCurrentPattern()));
	}

	emit currentPatternChanged();
}


const SampleTCO* SampleEditorWindow::currentTCO()
{
	return m_editor->currentTCO();
}

void SampleEditorWindow::dropEvent( QDropEvent *_de )
{
	/*
	QString type = StringPairDrag::decodeKey( _de );
	QString val = StringPairDrag::decodeValue( _de );
	if( type == "automatable_model" )
	{
		AutomatableModel * mod = dynamic_cast<AutomatableModel *>(
				Engine::projectJournal()->
					journallingObject( val.toInt() ) );
		if (mod != nullptr)
		{
			bool added = m_editor->m_pattern->addObject( mod );
			if ( !added )
			{
				TextFloat::displayMessage( mod->displayName(),
							   tr( "Model is already connected "
							   "to this pattern." ),
							   embed::getIconPixmap( "Sample" ),
							   2000 );
			}
			setCurrentPattern( m_editor->m_pattern );
		}
	}
	*/
	update();
}

void SampleEditorWindow::dragEnterEvent( QDragEnterEvent *_dee )
{
	/*
	if (! m_editor->validPattern() ) {
		return;
	}
	StringPairDrag::processDragEnterEvent( _dee, "automatable_model" );
	*/
}

void SampleEditorWindow::open(SampleTCO* tco)
{
	setCurrentTCO(tco);
	parentWidget()->show();
	show();
	setFocus();
}

QSize SampleEditorWindow::sizeHint() const
{
	return {INITIAL_WIDTH, INITIAL_HEIGHT};
}

void SampleEditorWindow::clearCurrentPattern()
{
	m_editor->m_tco = nullptr;
	setCurrentTCO(nullptr);
}

void SampleEditorWindow::focusInEvent(QFocusEvent * event)
{
	m_editor->setFocus( event->reason() );
}

void SampleEditorWindow::play()
{
	m_editor->play();
	setPauseIcon(Engine::getSong()->isPlaying());
}

void SampleEditorWindow::stop()
{
	m_editor->stop();
}

void SampleEditorWindow::reverse()
{
	m_editor->reverse();
}

void SampleEditorWindow::updateWindowTitle()
{
	if (m_editor->m_tco == nullptr)
	{
		setWindowTitle( tr( "Sample Editor - no sample" ) );
		return;
	}

	setWindowTitle( tr( "Sample Editor - %1" ).arg( m_editor->m_tco->name() ) );
}


bool SampleEditorWindow::isRecording() const
{
	return m_editor->isRecording();
}



void SampleEditorWindow::record()
{
	m_editor->record();
}




void SampleEditorWindow::recordAccompany()
{
	m_editor->recordAccompany();
}

void SampleEditorWindow::stopRecording()
{
	m_editor->stopRecording();
}