//
// C++ Implementation: fmactivate
//
// Description: 
//
//
// Author: Pierre Marchand <pierremarc@oep-h.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "fmactivate.h"

#include "fmfontdb.h" 
#include "fontitem.h"
#include "typotek.h"

#include <QDebug>
#include <QFile>
#include <QDomDocument>
#include <QDomNodeList>
#include <QDomElement>



FMActivate* FMActivate::instance = 0;

FMActivate::FMActivate()
{
	setErrorStrings();
}

void FMActivate::setErrorStrings()
{
	//: Activation subroutine failed to make a symbolic link to the font file
	errorStrings[NO_LINK] = tr("Unable to link");
	//: The Font asked for activation is already activated
	errorStrings[ALREADY_ACTIVE] = tr("Font already activated");
	//: Activation subroutine failed to remove a symbolic link to the font file
	errorStrings[NO_UNLINK] = tr("Unable to un-link");
	//: The Font asked for de-activation is already de-activated
	errorStrings[ALREADY_UNACTIVE] = tr("Font already de-activated");
	//: A postcript font (pfb) without its metrics file (afm)
	errorStrings[MISSING_AFM] = tr("Cannot link or copy the metrics file");
	//: A generic error in activation or deactivation process
	errorStrings[ERROR] = tr("Error", "activation");
}

FMActivate * FMActivate::getInstance()
{
	if(!instance)
	{
		instance = new FMActivate;
		Q_ASSERT(instance);
	}
	return instance;
}

#ifdef PLATFORM_APPLE

void FMActivate::activate(QList<FontItem*> fitList, bool act)
{
	// TODO insert error messages.
	QMap<FontItem*, bool> stack;
	typotek *T(typotek::getInstance());
	foreach(FontItem * fit , fitList)
	{
		qDebug() << "Activation of " << fit->path() << act;
		if ( act ) // Activation
		{

			if ( !T->isSysFont(fit) )
			{
				if ( !fit->isActivated() )
				{
					fit->setActivated ( true );

					// 				QFileInfo fofi ( fit->path() );

					if ( !QFile::copy ( fit->path() , T->getManagedDir() + "/" + fit->activationName() ) )
					{
						qDebug() << "unable to copy " << fit->path() ;
					}
					else
					{
						// Success
						stack[fit] = true;
						qDebug() << fit->path() << " copied" ;
						if ( !fit->afm().isEmpty() )
						{

							// 						QFileInfo afm ( fit->afm() );
							if ( !QFile::copy( fit->afm(), T->getManagedDir() + "/" + fit->activationAFMName() ) )
							{
								qDebug() << "unable to copy " << fit->afm();
							}
							else
							{
								qDebug() << fit->afm() << "copied";
							}
						}
						else
						{
							qDebug()<<"There is no AFM file attached to "<<fit->path();
						}
					}
				}
				else
				{
					qDebug() << "\tYet activated";
				}

			}
			else
			{
				qDebug() << "\tIs Locked";
			}

		}
		else // Deactivation
		{

			if ( !T->isSysFont(fit) )
			{
				if ( fit->isActivated() )
				{
					fit->setActivated ( false );
					// 				QFileInfo fofi ( fit->path() );
					if ( !QFile::remove ( T->getManagedDir() + "/" + fit->activationName() ) )
					{
						qDebug() << "unable to unlink " << fit->name() ;
					}
					else
					{
						// Success
						stack[fit] = false;
						if ( !fit->afm().isEmpty() )
						{
							// 						QFileInfo afm ( fit->afm() );
							if ( !QFile::remove ( T->getManagedDir() + "/" + fit->activationAFMName() ) )
							{
								qDebug() << "unable to unlink " << fit->afm() ;
							}
						}
						// 					typo->adaptator()->private_signal ( 0, fofi.fileName() );
					}
				}

			}
			else
			{
				qDebug() << "\tIs Locked";
			}
		}
	}

	QStringList aList;
	FMFontDb::DB()->TransactionBegin();
	foreach(FontItem* f, stack.keys())
	{
		f->setActivated(stack[f]);
		aList << f->path();
	}
	FMFontDb::DB()->TransactionEnd();

	emit activationEvent ( aList );
}

#elif _WIN32

void FMActivate::activate(QList<FontItem*> fitList, bool act)
{
	//TODO implement activation/deactivation for Windows
}

#else // fontconfig

void FMActivate::activate(QList< FontItem * > fitList, bool act)
{
	QMap<FontItem*, bool> stack;
	typotek *T(typotek::getInstance());
	foreach(FontItem * fit , fitList)
	{
		if ( act ) // Activation
		{
			if ( !T->isSysFont(fit) )
			{
				if ( !fit->isActivated() )
				{	
					if ( !QFile::link ( fit->path() , T->getManagedDir() + "/" + fit->activationName() ) )
					{
						qDebug() << "unable to link " << fit->path() ;
						m_errors[fit->path()] =  errorStrings[NO_LINK];
					}
					else
					{
						// Success
						stack[fit] = true;
						qDebug() << fit->path() << " linked" ;
						if ( !fit->afm().isEmpty() )
						{
							if ( !QFile::link ( fit->afm(), T->getManagedDir() + "/" + fit->activationAFMName() ) )
							{
								qDebug() << "unable to link " << fit->afm();
								m_errors[fit->path()] =  errorStrings[MISSING_AFM];
							}
							else
							{
								qDebug() << fit->afm() << " linked"; 
							}
						}
						else
						{
							qDebug()<<"There is no AFM file attached to "<<fit->path();
						}
					}
				}
				else
				{
					qDebug() << "\tYet activated";
					m_errors[fit->path()] = errorStrings[ALREADY_ACTIVE];
				}
	
			}
			else
			{
				remFcReject(fit->path());
			}
	
		}
		else // Deactivation
		{
			if ( !T->isSysFont(fit) )
			{
				if ( fit->isActivated() )
				{
					if ( !QFile::remove ( T->getManagedDir() + "/" + fit->activationName() ) )
					{
						qDebug() << "unable to unlink " << fit->name() ;
						m_errors[fit->path()] =  errorStrings[NO_UNLINK];
					}
					else
					{
						stack[fit] = false;
						if ( !fit->afm().isEmpty() )
						{
							if ( !QFile::remove ( T->getManagedDir() + "/" + fit->activationAFMName() ) )
							{
								qDebug() << "unable to unlink " << fit->afm() ;
								// if having warnings would not be over done, it would be a warning!
								m_errors[fit->afm()] =  errorStrings[NO_UNLINK];
							}
						}
					}
				}
				else
				{
					m_errors[fit->path()] = errorStrings[ALREADY_UNACTIVE];
				}
	
			}
			else
			{
				addFcReject(fit->path());
				
			}
		}
	}
	
	QStringList aList;
	FMFontDb::DB()->TransactionBegin();
	foreach(FontItem* f, stack.keys())
	{
		f->setActivated(stack[f]);
		aList << f->path();
	}
	FMFontDb::DB()->TransactionEnd();

	emit activationEvent(aList);
}

#endif

bool FMActivate::addFcReject(const QString & path)
{
#ifdef HAVE_FONTCONFIG
	QFile fcfile ( QDir::homePath() + "/.fonts.conf" );
	if ( !fcfile.open ( QFile::ReadWrite ) )
	{
		qWarning()<<"Cannot open"<< fcfile.fileName();
		return false;
	}
	else
	{
		QDomDocument fc ( "fontconfig" );
		fc.setContent ( &fcfile );
		QDomNodeList sellist = fc.elementsByTagName ( "selectfont" );
		// First we search if there’s yet an entry for path
		if(!sellist.isEmpty())
		{
			for ( int s(0); s < sellist.count(); ++s )
			{
				QDomNodeList rejectlist( sellist.at(s).toElement().elementsByTagName("rejectfont") );
				if(!rejectlist.isEmpty())
				{
					for( int r(0); r < rejectlist.count(); ++r )
					{
						QDomNodeList globlist(rejectlist.at(r).toElement().elementsByTagName("glob"));
						if(!globlist.isEmpty())
						{
							for( int g(0); g < globlist.count(); ++g )
							{
								QString t( globlist.at(g).toElement().text() );
								if(t == path)
								{
									qDebug()<<"Already here";
									return true;
								}
							}
							
						}
					}
				}
			}
		}
		
		// Now we can write in the first place available
		if(!sellist.isEmpty())
		{
			QDomNodeList rejectlist( sellist.at(0).toElement().elementsByTagName("rejectfont") );
			if(!rejectlist.isEmpty())
			{
// 				QDomNodeList globlist( rejectlist.at(0).toElement().elementsByTagName("glob") );
// 				if(!globlist.isEmpty())
// 				{
// 					QDomText pathelem = fc.createTextNode( path );
// 					globlist.at(0).toElement().appendChild(pathelem);
// 				}
// 				else
// 				{
					QDomElement globelem = fc.createElement ( "glob" );
					QDomText pathelem = fc.createTextNode( path );
					globelem.appendChild(pathelem);
					rejectlist.at(0).toElement().appendChild(globelem);
// 				}
			}
			else
			{
				QDomElement rejelem = fc.createElement ( "rejectfont" );
				QDomElement globelem = fc.createElement ( "glob" );
				QDomText pathelem = fc.createTextNode( path );
				globelem.appendChild(pathelem);
				rejelem.appendChild(globelem);
				sellist.at(0).toElement().appendChild(rejelem);
			}
		}
		else
		{
			QDomElement root = fc.documentElement();
			QDomElement selelem = fc.createElement ( "selectfont" );
			QDomElement rejelem = fc.createElement ( "rejectfont" );
			QDomElement globelem = fc.createElement ( "glob" );
			QDomText pathelem = fc.createTextNode( path );
			globelem.appendChild(pathelem);
			rejelem.appendChild(globelem);
			selelem.appendChild(rejelem);
			root.appendChild(selelem);
		}
		
		fcfile.resize ( 0 );
		QTextStream ts ( &fcfile );
		fc.save ( ts,4 );
		fcfile.close();
	}
#endif //  HAVE_FONTCONFIG
	return true;
}

bool FMActivate::remFcReject(const QString & path)
{
#ifdef HAVE_FONTCONFIG
	QFile fcfile ( QDir::homePath() + "/.fonts.conf" );
	if ( !fcfile.open ( QFile::ReadWrite ) )
	{
		return false;
	}
	else
	{
		QDomDocument fc ( "fontconfig" );
		fc.setContent ( &fcfile );
		QDomNodeList sellist = fc.elementsByTagName ( "selectfont" );
		
		if(!sellist.isEmpty())
		{
			for ( int s(0); s < sellist.count(); ++s )
			{
				QDomNodeList rejectlist( sellist.at(s).toElement().elementsByTagName("rejectfont") );
				if(!rejectlist.isEmpty())
				{
					for( int r(0); r < rejectlist.count(); ++r )
					{
						QDomNodeList globlist(rejectlist.at(r).toElement().elementsByTagName("glob"));
						if(!globlist.isEmpty())
						{
							for( int g(0); g < globlist.count(); ++g )
							{
								QString t( globlist.at(g).toElement().text() );
								if(t == path)
								{
									rejectlist.at(r).removeChild(globlist.at(g).toElement());
									fcfile.resize ( 0 );
									QTextStream ts ( &fcfile );
									fc.save ( ts,4 );
									fcfile.close();
									return true;
								}
							}
							
						}
					}
				}
			}
		}
	}
#endif //  HAVE_FONTCONFIG
	return true;
}

QMap<QString,QString> FMActivate::errors()
{
	QMap<QString,QString> ret(m_errors);
	m_errors.clear();
	return ret;
}
