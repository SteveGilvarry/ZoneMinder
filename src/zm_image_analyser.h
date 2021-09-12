#ifndef ZM_IMAGE_ANALYSER_H
#define ZM_IMAGE_ANALYSER_H



#include <list>
#include <string>
#include <stdexcept>
#include <memory>
#include <fstream>

#include "zm.h"
#include "zm_monitor.h"
#include "zm_image.h"
#include "zm_zone.h"
#include "zm_event.h"
#include "zm_detector.h"


using namespace std;
//! List of available detectors.
typedef std::list<Detector *> DetectorsList;

//! A structure to store the general configuration of a plugin
struct pGenConf {
    bool Registered;
    bool Configured;
    pGenConf():
        Registered(false),
        Configured(false)
    {}
};

//! A structure to store the zone configuration of a plugin
struct pZoneConf {
    bool Enabled;
    bool RequireNatDet;
    bool IncludeNatDet;
    bool ReInitNatDet;
    pZoneConf():
        Enabled(false),
        RequireNatDet(false),
        IncludeNatDet(false),
        ReInitNatDet(false)
    {}
};

//! Map of zone configuration for a plugin
typedef std::map<unsigned int,pZoneConf> PluginZoneConf;

//! Class for handling image detection.
class ImageAnalyser {
  public:

  //!Default constructor.
  ImageAnalyser() {};

  //! Destructor.
  ~ImageAnalyser();

  //! Copy constructor.
  ImageAnalyser(const ImageAnalyser& source);

  //! Overloaded operator=.
  ImageAnalyser& operator=(const ImageAnalyser& source);

    //! Adds new plugin's detector to the list of detectors.
    void addDetector(std::unique_ptr<Detector> Det)
    {
        m_Detectors.push_back(Det.release());
    }

    void onCreateEvent(Zone** zones, Event* event);
    void onCloseEvent(Zone** zones, Event* event);

    //! Do detection in an image by calling all available detectors.
    int DoDetection(const Image &comp_image, Zone** zones, int n_numZones, Event::StringSetMap noteSetMap, std::string& det_cause);

    //! Configure all loaded plugins using given configuration file.
    void configurePlugins(std::string sConfigFileName, bool bDoNativeDet = 0);

    //! Check if the configuration file contains the right section name
    bool isValidConfigFile(std::string sPluginName, std::string sConfigFileName);

    //! Get index of enabled zones for this monitor (same ordering as in Monitor::Load)
    bool getMonitorZones();

    //! Get plugin configuration from database
    bool getPluginConfig(std::string sPluginName, std::vector<unsigned int> vnPluginZones, std::map<unsigned int,std::map<std::string,std::string> >& mapPluginConf);

    //! Get enabled zones for the plugin
    bool getEnabledZonesForPlugin(std::string sPluginName, std::vector<unsigned int>& vnPluginZones);

    //! Get zones configuration from database
    bool getZonesConfig(std::string sLoadedPlugins);

    //! Get Zone configuration from this class
    bool getZoneConfig(unsigned int nZone, zConf& zoneConf);

    //! Get the general settings of a registered plugin
    bool getRegPluginGenConf(std::string sPluginName, pGenConf& regPluginGenConf);

    //! Get the zone settings of a registered plugin
    void getRegPluginZoneConf(std::string sPluginName, PluginZoneConf& regPluginZoneConf);

    //! Remove from db plugins no longer detected
    void cleanupPlugins();

  private:

    //! All available detectors.
    DetectorsList m_Detectors;

    //! The monitor id
    Monitor *monitor;

    //! Native detection is enabled
    bool m_bIsNativeDetEnabled;

    //! Analyser is enabled
    bool m_bIsAnalyserEnabled;

    //! A structure to store a plugin parameter
    struct zIdName {
        unsigned int zoneId;
        std::string name;
    };

    //! A vector filled with parameters of zones
    std::vector<zConf> m_vZonesConfig;

    //! A structure to store basic settings of a zone
    struct zSetting {
        unsigned int id;
        std::string name;
        std::string type;
    };

    //! A vector filled with settings of zones enabled for the monitor
    std::vector<zSetting> m_vMonitorZones;

    //! A map to store the general configuration of registered plugins
    std::map<std::string,pGenConf> mapRegPluginGenConf;

    //! A map to store the zone configuration of registered plugins
    std::map<std::string,PluginZoneConf> mapRegPluginZoneConf;
};



#endif //ZM_IMAGE_ANALYSER_H
