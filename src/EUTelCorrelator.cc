// -*- mode: c++; mode: auto-fill; mode: flyspell-prog; -*-
// Author Silvia Bonfanti, Uni. Insubria  <mailto:silviafisica@gmail.com>
// Author Loretta Negrini, Uni. Insubria  <mailto:loryneg@gmail.com>
// Version $Id: EUTelCorrelator.cc,v 1.3 2008-07-21 17:04:27 bulgheroni Exp $
/*
 *   This source code is part of the Eutelescope package of Marlin.
 *   You are free to use this source files for your own development as
 *   long as it stays in a public research context. You are not
 *   allowed to use it for commercial purpose. You must put this
 *   header with author names in all development based on this file.
 *
 */

// built only if GEAR is used
#ifdef USE_GEAR

// eutelescope includes ".h" 
#include "EUTelCorrelator.h"
#include "EUTelRunHeaderImpl.h"
#include "EUTelEventImpl.h"
#include "EUTELESCOPE.h"
#include "EUTelVirtualCluster.h"
#include "EUTelFFClusterImpl.h"
#include "EUTelExceptions.h"

// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/Global.h"

// aida includes <.h>
#ifdef MARLIN_USE_AIDA
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/IHistogram2D.h>
#include <AIDA/ICloud2D.h>
#include <AIDA/ITree.h>
#endif

// gear includes <.h>
#include <gear/GearMgr.h>
#include <gear/SiPlanesParameters.h>

// lcio includes <.h> 
#include <IMPL/LCCollectionVec.h>
#include <IMPL/TrackerPulseImpl.h>
#include <IMPL/TrackerHitImpl.h>
#include <UTIL/CellIDDecoder.h>

// system includes <>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace marlin;
using namespace eutelescope;

// definition of static members mainly used to name histograms
#ifdef MARLIN_USE_AIDA
std::string EUTelCorrelator::_clusterXCorrelationHistoName   = "ClusterXCorrelationHisto";
std::string EUTelCorrelator::_hitXCorrelationCloudName       = "HitXCorrelationCloud";
std::string EUTelCorrelator::_hitYCorrelationCloudName       = "HitYCorrelationCloud";
std::string EUTelCorrelator::_clusterYCorrelationHistoName   = "ClusterYCorrelationHisto";
#endif

EUTelCorrelator::EUTelCorrelator () : Processor("EUTelCorrelator") {

  // modify processor description
  _description =
    "EUTelCorrelator fills histograms with correlation plots";

  registerInputCollection(LCIO::TRACKERPULSE,"InputClusterCollectionName",
			  "Cluster (pulse) collection name",
			  _inputClusterCollectionName, string ( "cluster" ));
  
  registerInputCollection(LCIO::TRACKERHIT,"InputHitCollectionName",
			  "Hit collection name",
			  _inputHitCollectionName, string ( "hit" ));
  
  
}


void EUTelCorrelator::init() {
  // this method is called only once even when the rewind is active
  // usually a good idea to
  printParameters ();

  // set to zero the run and event counters
  _iRun = 0;
  _iEvt = 0;

#ifndef USE_GEAR

  streamlog_out ( ERROR4 ) <<  "Marlin was not built with GEAR support." << endl
			   <<  "You need to install GEAR and recompile Marlin with -DUSE_GEAR before continue." << endl;
  
  // I'm thinking if this is the case of throwing an exception or
  // not. This is a really error and not something that can
  // exceptionally happens. Still not sure what to do
  exit(-1);

#else

  // check if the GEAR manager pointer is not null!
  if ( Global::GEAR == 0x0 ) {
    streamlog_out ( ERROR4 ) <<  "The GearMgr is not available, for an unknown reason." << endl;
    exit(-1);
  }

  _siPlanesParameters  = const_cast<gear::SiPlanesParameters* > (&(Global::GEAR->getSiPlanesParameters()));
  _siPlanesLayerLayout = const_cast<gear::SiPlanesLayerLayout*> ( &(_siPlanesParameters->getSiPlanesLayerLayout() ));

#endif

  

}

void EUTelCorrelator::processRunHeader (LCRunHeader * rdr) {


  EUTelRunHeaderImpl * runHeader = new EUTelRunHeaderImpl( rdr ) ; 
  
  _noOfDetectors = runHeader->getNoOfDetector(); 

  // the four vectors containing the first and the last pixel
  // along both the directions
  _minX = runHeader->getMinX();
  _maxX = runHeader->getMaxX();
  _minY = runHeader->getMinY();
  _maxY = runHeader->getMaxY();


  // the run header contains the number of detectors. This number
  // should be in principle be the same as the number of layers in the
  // geometry description
  if ( runHeader->getNoOfDetector() != _siPlanesParameters->getSiPlanesNumber() ) {
    streamlog_out ( ERROR4 ) << "Error during the geometry consistency check: " << endl
			     << "The run header says there are " << runHeader->getNoOfDetector() << " silicon detectors " << endl
			     << "The GEAR description says     " << _siPlanesParameters->getSiPlanesNumber() << " silicon planes" << endl;
    exit(-1);
  }
  
  // this is the right place also to check the geometry ID. This is a
  // unique number identifying each different geometry used at the
  // beam test. The same number should be saved in the run header and
  // in the xml file. If the numbers are different, instead of barely
  // quitting ask the user what to do.

  if ( runHeader->getGeoID() == 0 ) 
    streamlog_out ( WARNING0 ) <<  "The geometry ID in the run header is set to zero." << endl
			       <<  "This may mean that the GeoID parameter was not set" << endl;
  
  
  if ( runHeader->getGeoID() != _siPlanesParameters->getSiPlanesID() ) {
    streamlog_out ( ERROR1 ) <<  "Error during the geometry consistency check: " << endl
			     << "The run header says the GeoID is " << runHeader->getGeoID() << endl
			     << "The GEAR description says is     " << _siPlanesParameters->getSiPlanesID() << endl;
    string answer;
    while (true) {
      streamlog_out ( ERROR1 ) << "Type Q to quit now or C to continue using the actual GEAR description anyway [Q/C]" << endl;
      cin >> answer;
      // put the answer in lower case before making the comparison.
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
  	exit(-1);
      } else if ( answer == "c" ) {
  	break;
      }
    }
  }


  delete runHeader;

  // increment the run counter
  ++_iRun;
}


void EUTelCorrelator::processEvent (LCEvent * event) {

#ifdef MARLIN_USE_AIDA

  if (_iEvt % 10 == 0) 
    streamlog_out( MESSAGE4 ) << "Processing event " 
			      << setw(6) << setiosflags(ios::right) << event->getEventNumber() << " in run "
			      << setw(6) << setiosflags(ios::right) << setfill('0')  << event->getRunNumber() 
			      << setfill(' ') << " (Total = " << setw(10) << _iEvt << ")" 
			      << resetiosflags(ios::left) << endl;
  ++_iEvt;


  EUTelEventImpl * evt = static_cast<EUTelEventImpl*> (event) ;
  
  if ( evt->getEventType() == kEORE ) {
    streamlog_out ( DEBUG4 ) << "EORE found: nothing else to do." << endl;
    return;
  } else if ( evt->getEventType() == kUNKNOWN ) {
    streamlog_out ( WARNING2 ) << "Event number " << evt->getEventNumber() << " in run " << evt->getRunNumber()
			       << " is of unknown type. Continue considering it as a normal Data Event." << endl;
  }
  // if the Event that we are looking is the first we create files
  // with histograms.
  if ( isFirstEvent() ) {

    try {
    
      event->getCollection( _inputClusterCollectionName );

      _hasClusterCollection = true;

    } catch ( lcio::Exception& e ) {
      
      _hasClusterCollection = false;
    }

    try {
    
      event->getCollection( _inputHitCollectionName ) ;

      _hasHitCollection = true;

    } catch ( lcio::Exception& e ) {
      
      _hasHitCollection = false;
    }

    bookHistos();

    _isFirstEvent = false;

  }

  

  try {

    if ( _hasClusterCollection ) {

      LCCollectionVec * inputClusterCollection   = static_cast<LCCollectionVec*> 
	(event->getCollection( _inputClusterCollectionName ));
      
      CellIDDecoder<TrackerPulseImpl>  pulseCellDecoder( inputClusterCollection );
      
      
      for ( size_t iExt = 0 ; iExt < inputClusterCollection->size() ; ++iExt ) {
	
	TrackerPulseImpl * externalPulse = static_cast< TrackerPulseImpl * > 
	  ( inputClusterCollection->getElementAt( iExt ) );
	
	EUTelVirtualCluster  * externalCluster;
	
	ClusterType type = static_cast<ClusterType>(static_cast<int>((pulseCellDecoder(externalPulse)["type"])));
	
	if ( type == kEUTelFFClusterImpl ) {
	  externalCluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl*>
						    ( externalPulse->getTrackerData()) );
	} else {
	  streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
	  throw UnknownDataTypeException("Cluster type unknown");
	}
	
	int externalSensorID = pulseCellDecoder( externalPulse ) [ "sensorID" ] ;
	
	float externalXCenter;
	float externalYCenter;
	externalCluster->getCenterOfGravity( externalXCenter, externalYCenter ) ; 

	for ( size_t iInt = 0;  iInt <  inputClusterCollection->size() ; ++iInt ) {

	  TrackerPulseImpl * internalPulse = static_cast< TrackerPulseImpl * > 
	    ( inputClusterCollection->getElementAt( iInt ) );
	  EUTelVirtualCluster  * internalCluster;
	  ClusterType type = static_cast<ClusterType>
	    (static_cast<int>((pulseCellDecoder(internalPulse)["type"])));
	  if ( type == kEUTelFFClusterImpl ) {
	    internalCluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl*> 
						      (internalPulse->getTrackerData()) );
	  } else {
	    streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
	    throw UnknownDataTypeException("Cluster type unknown");
	  }
	  int internalSensorID = pulseCellDecoder( internalPulse ) [ "sensorID" ] ;

	  if ( internalSensorID != externalSensorID ) {

	    float internalXCenter;
	    float internalYCenter;
	    internalCluster->getCenterOfGravity( internalXCenter, internalYCenter ) ; 

	    streamlog_out ( DEBUG ) << "Filling histo " << externalSensorID << " " << internalSensorID << endl;
	  
	    _clusterXCorrelationMatrix[ externalSensorID ][ internalSensorID ]->fill( externalXCenter, internalXCenter );
	    _clusterYCorrelationMatrix[ externalSensorID ][ internalSensorID ]->fill( externalYCenter, internalYCenter ); 
	  
	  }

	}
      }

    }

    if ( _hasHitCollection ) {

      LCCollectionVec * inputHitCollection = static_cast< LCCollectionVec *> 
	( event->getCollection( _inputHitCollectionName )) ;
      
      LCCollectionVec * originalDataCollectionVec = dynamic_cast< LCCollectionVec * > (evt->getCollection( "original_zsdata" ) );
      
      CellIDDecoder<TrackerDataImpl> originalCellDecoder( originalDataCollectionVec );

      for ( size_t iExt = 0 ; iExt < inputHitCollection->size(); ++iExt ) {

	// this is the external hit
	TrackerHitImpl * externalHit = static_cast< TrackerHitImpl * > ( inputHitCollection->getElementAt( iExt ) );
	
	// now let me take the vector of clusters used to define this
	// hit
	LCObjectVec      externalClusterVec = externalHit->getRawHits();
	
	// actually there is only one cluster in this vector
	TrackerDataImpl * externalCluster = dynamic_cast< TrackerDataImpl *> ( externalClusterVec[ 0 ] );
	
	int externalSensorID = originalCellDecoder( externalCluster ) ["sensorID"] ;

	double * externalPosition;
	externalPosition = (double *) externalHit->getPosition();
      
	for ( size_t iInt = 0; iInt < inputHitCollection->size(); ++iInt ) {

	  TrackerHitImpl  * internalHit = static_cast< TrackerHitImpl * > ( inputHitCollection->getElementAt( iInt ) );
	  
	  LCObjectVec       internalClusterVec = internalHit->getRawHits();
	  TrackerDataImpl * internalCluster    = dynamic_cast<TrackerDataImpl * > ( internalClusterVec[ 0 ] );
	  
	  int internalSensorID = originalCellDecoder( internalCluster ) [ "sensorID" ];

	  if ( internalSensorID != externalSensorID ) {
	    
	    double * internalPosition;
	    internalPosition = (double *) internalHit->getPosition(  );
	    
	    _hitXCorrelationMatrix[ externalSensorID ] [ internalSensorID ] ->fill ( externalPosition[0] , internalPosition[0] ) ;
	   


	  }
	  
	}
	
      }
    }


  } catch (DataNotAvailableException& e  ) {
   
    streamlog_out  ( WARNING2 ) <<  "No input collection found on event " << event->getEventNumber() 
				<< " in run " << event->getRunNumber() << endl;
  }

#endif 

}

void EUTelCorrelator::end() {
  
  streamlog_out ( MESSAGE4 )  << "Successfully finished" << endl;
}

void EUTelCorrelator::bookHistos() {
  
  if ( !_hasClusterCollection && !_hasHitCollection ) return ;

#ifdef MARLIN_USE_AIDA

  try {

    streamlog_out ( MESSAGE4 ) <<  "Booking histograms" << endl;
    
    // create all the directories first
    vector< string > dirNames;

    if ( _hasClusterCollection ) {
      dirNames.push_back ("ClusterX");
      dirNames.push_back ("ClusterY");
    }
    if ( _hasHitCollection ) {
      dirNames.push_back ("HitX");
      dirNames.push_back ("HitY");
    }

    for ( size_t iPos = 0 ; iPos < dirNames.size() ; iPos++ ) {

      AIDAProcessor::tree(this)->mkdir( dirNames[iPos].c_str() ) ;

    }
     
    
    string tempHistoName;
    string tempHistoTitle ;

    
    for ( int row = 0 ; row < _noOfDetectors; ++row ) {

      vector< AIDA::IHistogram2D * > innerVectorXCluster;
      vector< AIDA::IHistogram2D * > innerVectorYCluster;

      map< unsigned int , AIDA::ICloud2D * > innerMapXHit;
      // vector< AIDA::ICloud2D * > innerVectorXHit;
      vector< AIDA::ICloud2D * > innerVectorYHit;
      
      
      for ( int col = 0 ; col < _noOfDetectors; ++col ) {

	if ( col != row ) {
	  
	  //we create histograms for X and Y Cluster correlation 
	  if ( _hasClusterCollection ) {
	    {
	      stringstream ss;
	      ss << "ClusterX/" <<  _clusterXCorrelationHistoName << "-d" << row 
		 << "-d" << col ;
	      
	      tempHistoName = ss.str();
	    }
	    
	    streamlog_out( DEBUG ) << "Booking histo " << tempHistoName << endl;
	    
	    int     xBin = _maxX[ col ] - _minX[ col ] + 1;
	    double  xMin = static_cast<double >(_minX[ col ]) - 0.5;
	    double  xMax = static_cast<double >(_maxX[ col ]) + 0.5;
	    int     yBin = _maxX[ row ] - _minX[ row ] + 1;
	    double  yMin = static_cast<double >(_minX[ row ]) - 0.5;
	    double  yMax = static_cast<double >(_maxX[ row ]) + 0.5;
	    
	    AIDA::IHistogram2D * histo2D = 
	      AIDAProcessor::histogramFactory(this)->createHistogram2D( tempHistoName.c_str(), 
									xBin, xMin, xMax, yBin, yMin, yMax );
	    
	    {

	      stringstream tt ;
	      tt << "XClusterCorrelation" << "-d" << row 
		 << "-d" << col ;
	      tempHistoTitle = tt.str();
	     
	    }

	    histo2D->setTitle( tempHistoTitle.c_str() );
	    innerVectorXCluster.push_back ( histo2D );
	    
	    
	    {
	      stringstream ss;
	      ss << "ClusterY/" <<  _clusterYCorrelationHistoName << "-d" << row 
		 << "-d" << col ;
	    
	      tempHistoName = ss.str();
	    }
	  
	    streamlog_out( DEBUG ) << "Booking histo " << tempHistoName << endl;
	  
	    xBin = _maxY[ col ] - _minY[ col ] + 1;
	    xMin = static_cast<double >(_minY[ col ]) - 0.5;
	    xMax = static_cast<double >(_maxY[ col ]) + 0.5;
	    yBin = _maxY[ row ] - _minY[ row ] + 1;
	    yMin = static_cast<double >(_minY[ row ]) - 0.5;
	    yMax = static_cast<double >(_maxY[ row ]) + 0.5;
	  
	  
	    histo2D =
	      AIDAProcessor::histogramFactory(this)->createHistogram2D( tempHistoName.c_str(), 
									xBin, xMin, xMax, yBin, yMin, yMax );
	    {
	      stringstream tt ;
	      tt << "YClusterCorrelation" << "-d" << row 
		 << "-d" << col ;
	      tempHistoTitle = tt.str();
	    }

	    histo2D->setTitle( tempHistoTitle.c_str()) ;
	       
	    innerVectorYCluster.push_back ( histo2D );
	     
	  }

	  //we create clouds for X and Y Hit correlation
	  if ( _hasHitCollection ) {


	    {
	      stringstream ss;
	      ss << "HitX/" << _hitXCorrelationCloudName << "-d" << row << "-d" << col;
	      tempHistoName = ss.str();

	    }

	    streamlog_out( MESSAGE ) << "Booking cloud " << tempHistoName << endl;
	    
	    AIDA::ICloud2D * cloud2D = 
	      AIDAProcessor::histogramFactory( this )->createCloud2D( tempHistoName.c_str() );

	    string title ;
	    stringstream tt ;
	    tt << "XHitCorrelation" << "-d" << row 
	       << "-d" << col ;
	    title = tt.str();

	    cloud2D->setTitle( title ) ;
	    innerMapXHit[ _siPlanesLayerLayout->getID( col ) ] =  cloud2D ;


	    {
	      stringstream ss;
	      ss << "HitY/" << _hitYCorrelationCloudName << "-d" << row << "-d" << col;
	      tempHistoName = ss.str();

	    }

	    streamlog_out( DEBUG ) << "Booking cloud " << tempHistoName << endl;
	    
	    
	    AIDAProcessor::histogramFactory( this )->createCloud2D( tempHistoName.c_str() );
	    
	    
	    tt << "YHitCorrelation" << "-d" << row 
	       << "-d" << col ;
	    title = tt.str();

	    cloud2D->setTitle( title ) ;
	    innerVectorYHit.push_back( cloud2D );
	  }

	} else {
	  
	  if ( _hasClusterCollection ) {
	    innerVectorXCluster.push_back( NULL );
	    innerVectorYCluster.push_back( NULL );
	  }

	  if ( _hasHitCollection ) {
	    innerMapXHit[ _siPlanesLayerLayout->getID( col )  ] = NULL ;
	    //	  innerVectorXHit.push_back( NULL );
	    innerVectorYHit.push_back( NULL );
	  }

	}

      }

      if ( _hasClusterCollection ) {
	_clusterXCorrelationMatrix.push_back( innerVectorXCluster ) ;
	_clusterYCorrelationMatrix.push_back( innerVectorYCluster ) ;
      }

      if ( _hasHitCollection ) {
	_hitXCorrelationMatrix[ _siPlanesLayerLayout->getID( row ) ] = innerMapXHit;
	
	// _hitXCorrelationMatrix.push_back( innerVectorXHit ) ;
	_hitYCorrelationMatrix.push_back( innerVectorYHit ) ;
      }
    }

 

  
  } catch (lcio::Exception& e ) {
    
    streamlog_out ( ERROR1 ) << "No AIDAProcessor initialized. Sorry for quitting..." << endl;
    exit( -1 ); 

  }
#endif
}

  

#endif
