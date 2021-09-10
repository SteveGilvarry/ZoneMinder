#include "zm_image_analyser.h"



/*!\fn ImageAnalyser::ImageAnalyser(const ImageAnalyser& source)
 * \param source is the object to copy
 */
ImageAnalyser::ImageAnalyser(const ImageAnalyser& source)
{
  m_Detectors = source.m_Detectors;
}



/*!\fn ImageAnalyser::operator=(const ImageAnalyser& source)
 * \param source is the object to copy
 */
ImageAnalyser& ImageAnalyser::operator=(const ImageAnalyser& source)
{
  m_Detectors = source.m_Detectors;
  return *this;
}



ImageAnalyser::~ImageAnalyser()
{
  for(DetectorsList::reverse_iterator It = m_Detectors.rbegin();
    It != m_Detectors.rend();
    ++It)
    delete *It;
}



/*!\fn ImageAnalyser::DoDetection(const Image &comp_image, Zone** zones, int n_numZones, Event::StringSetMap noteSetMap, std::string& det_cause)
 * \param comp_image is the image to analyse
 * \param zones is the zones array to analyse
 * \param n_numZones is the number of zones
 * \param noteSetMap is the map of events descriptions
 * \param det_cause is a string describing detection cause
 * \param score is the plugin score
 */
int ImageAnalyser::DoDetection(const Image &comp_image, Zone** zones, int n_numZones, Event::StringSetMap noteSetMap, std::string& det_cause, unsigned int& score)
{
  Event::StringSet zoneSet;
  int score = 0;

  for(DetectorsList::iterator It = m_Detectors.begin();
    It != m_Detectors.end();
    ++It)
  {
    int detect_score = (*It)->Detect(comp_image, zones, n_numZones, zoneSet);
    if (detect_score)
    {
      score += detect_score;
      noteSetMap[(*It)->getDetectionCause()] = zoneSet;
      if (det_cause.length())
        det_cause += ", ";
      det_cause += (*It)->getDetectionCause();
    }
  }
  return score;
}



/*!\fn ImageAnalyser::isValidConfigFile(std::string sPluginName, std::string sConfigFileName)
 * \param sPluginName is the name of the plugin (filename without extension)
 * \param sConfigFileName is the path to the configuration file which should include configuration directives for the plugin
 * \return true if the config file contains the right section name
 */
bool ImageAnalyser::isValidConfigFile(std::string sPluginName, std::string sConfigFileName)
{
    std::ifstream ifs(sConfigFileName.c_str());
    std::string line;
    bool rtnVal = false;
    while (getline(ifs, line))
    {
        if (line == "[" + sPluginName + "]")
        {
            rtnVal = true;
            break;
        }
    }
    ifs.close();
    return rtnVal;
}


/*!\fn ImageAnalyser::getMonitorZones()
 * \return true if at least a zone is configured for the monitor
 */
bool ImageAnalyser::getMonitorZones()
{
    static char sql[ZM_SQL_MED_BUFSIZ];

    // We use the same ordering as in Monitor::Load
    snprintf(sql, sizeof(sql), "SELECT `Id`, `Name`, `Type` FROM `Zones` WHERE `MonitorId` = %d ORDER BY `Type`, `Id`;", m_nMonitorId);

    if (mysql_query(&dbconn, sql))
    {
        Error("Can't run query: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    MYSQL_RES *result = mysql_store_result(&dbconn);
    if (!result)
    {
        Error("Can't use query result: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    if (mysql_num_rows(result) > 0)
    {
        for (unsigned int i = 0; MYSQL_ROW dbrow = mysql_fetch_row(result); i++)
        {
            if (mysql_errno(&dbconn))
            {
                Error("Can't fetch row: %s", mysql_error(&dbconn));
                exit(mysql_errno(&dbconn));
            }
            zSetting zone;
            zone.id = (unsigned int)strtoul(dbrow[0], NULL, 0);
            zone.name = std::string(dbrow[1]);
            zone.type = std::string(dbrow[2]);
            m_vMonitorZones.push_back(zone);
        }
    }
    mysql_free_result(result);

    return ( m_vMonitorZones.size() );
}



/*!\fn ImageAnalyser::getPluginConfig(std::string sPluginName, std::vector<unsigned int> vnPluginZones, std::map<unsigned int,std::map<std::string,std::string> >& mapPluginConf)
 * \param sPluginName is the name of the plugin (filename without extension)
 * \param vnPluginZones is a vector containing the index of zones enabled for the plugin (not the zone Id in the database)
 * \param mapPluginConf is the map filled with configuration parameters for the plugin
 * \return true if all found parameters are applied to the map
 */
bool ImageAnalyser::getPluginConfig(std::string sPluginName, std::vector<unsigned int> vnPluginZones, std::map<unsigned int,std::map<std::string,std::string> >& mapPluginConf)
{
    static char sql[ZM_SQL_MED_BUFSIZ];

    // Get plugin configuration parameters from `PluginsConfig` table
    snprintf(sql, sizeof(sql), "SELECT `ZoneId`, `Name`, `Value` FROM `PluginsConfig` WHERE `MonitorId`=%d AND `pluginName`='%s' ORDER BY `ZoneId` ASC;", m_nMonitorId, sPluginName.c_str());

    if (mysql_query(&dbconn, sql))
    {
        Error("Can't run query: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    MYSQL_RES *result = mysql_store_result(&dbconn);
    if (!result)
    {
        Error("Can't use query result: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    size_t nParamCnt = 0;
    size_t nParamNum = mysql_num_rows(result);

    if (nParamNum > 0)
    {
        std::vector<MYSQL_ROW> vRows;
        for (unsigned int i = 0; MYSQL_ROW dbrow = mysql_fetch_row(result); i++)
        {
            if (mysql_errno(&dbconn))
            {
                Error("Can't fetch row: %s", mysql_error(&dbconn));
                mysql_free_result(result);
                exit(mysql_errno(&dbconn));
            }
            vRows.push_back(dbrow);
        }
        // Iterate over the zones
        for (size_t i = 0; i < m_vMonitorZones.size(); i++)
        {
            // Iterate over the configuration parameters
            for (std::vector<MYSQL_ROW>::iterator it = vRows.begin(); it != vRows.end(); it++)
            {
                // Add the parameter to the map if the zone id is found
                if ( (unsigned int)strtoul((*it)[0], NULL, 0) == m_vMonitorZones[i].id )
                {
                    nParamCnt++;
                    std::string name((*it)[1]);
                    std::string value((*it)[2]);
                    if((name == "Enabled") && (value == "Yes")) {
                        mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].Enabled = true;
                    } else if((name == "RequireNatDet") && (value == "Yes")) {
                        mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].RequireNatDet = true;
                    } else if((name == "IncludeNatDet") && (value == "Yes")) {
                        mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].IncludeNatDet = true;
                    } else if((name == "ReInitNatDet") && (value == "Yes")) {
                        mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].ReInitNatDet = true;
                    }
                    // Keep only enabled zones in mapPluginConf
                    if (binary_search(vnPluginZones.begin(), vnPluginZones.end(), i)) {
                        mapPluginConf[i][name] = value;
                    }
                }
            }
            if ( mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].Enabled
                    && mapRegPluginZoneConf[sPluginName][m_vMonitorZones[i].id].RequireNatDet
                    && !m_bIsNativeDetEnabled )
                Warning("Plugin '%s' will never enter in alarm because native detection is required but not enabled", sPluginName.c_str());
        }
    }
    mysql_free_result(result);

    return ( nParamNum == nParamCnt );
}



/*!\fn ImageAnalyser::getEnabledZonesForPlugin(std::string sPluginName, std::vector<unsigned int>& vnPluginZones)
 * \param sPluginName is the name of the plugin (filename without extension)
 * \param vnPluginZones is the vector list filled with zones enabled for this plugin
 * \return true if at least one active or exclusive zone exist
 */
bool ImageAnalyser::getEnabledZonesForPlugin(std::string sPluginName, std::vector<unsigned int>& vnPluginZones)
{
    static char sql[ZM_SQL_MED_BUFSIZ];
    bool bPluginEnabled = false;
    std::string sZones;

    // Get the sorted list of zones ids which have the plugin enabled
    snprintf(sql, sizeof(sql), "SELECT `ZoneId` FROM `PluginsConfig` WHERE `MonitorId`=%d AND `pluginName`='%s' AND `Name`='Enabled' AND `Value`='yes' ORDER BY `ZoneId` ASC;", m_nMonitorId, sPluginName.c_str());

    if (mysql_query( &dbconn, sql))
    {
        Error("Can't run query: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    MYSQL_RES *result = mysql_store_result(&dbconn);
    if (!result)
    {
        Error("Can't use query result: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }

    if (mysql_num_rows(result) > 0)
    {
        std::vector<unsigned int> vnEnabledZoneIds;
        for (unsigned int i = 0; MYSQL_ROW dbrow = mysql_fetch_row(result); i++)
        {
            if (mysql_errno(&dbconn))
            {
                Error("Can't fetch row: %s", mysql_error(&dbconn));
                mysql_free_result(result);
                exit(mysql_errno(&dbconn));
            }
            vnEnabledZoneIds.push_back(atoi(dbrow[0]));
        }

        // Iterate over the zones
        for (size_t i = 0; i < m_vMonitorZones.size(); i++)
        {
            if (binary_search(vnEnabledZoneIds.begin(), vnEnabledZoneIds.end(), m_vMonitorZones[i].id))
            {
                // Add the index to the vector if the zone id is found
                vnPluginZones.push_back(i);
                std::string sZoneType = m_vMonitorZones[i].type;
                if ((sZoneType == "Active") || (sZoneType == "Exclusive"))
                    bPluginEnabled = true;
                if ( sZones.length() )
                    sZones += ", ";
                sZones += m_vMonitorZones[i].name + " (" + sZoneType + ")";
            }
        }
    }
    mysql_free_result(result);

    if (bPluginEnabled)
    {
        Info("Plugin '%s' is enabled for zone(s): %s", sPluginName.c_str(), sZones.c_str());
    }
    else
    {
        Info("Plugin '%s' is disabled (not enabled for any active or exclusive zones)", sPluginName.c_str());
    }
    return bPluginEnabled;
}


/*!\fn ImageAnalyser::getZonesConfig(std::string sLoadedPlugins)
 * \param sLoadedPlugins is the formatted list of loaded plugins
 */
bool ImageAnalyser::getZonesConfig(std::string sLoadedPlugins)
{
    static char sql[ZM_SQL_MED_BUFSIZ];

    if ( !sLoadedPlugins.length() ) return false;

    // Get the sorted list of zones and which have a setting enabled
    snprintf(sql, sizeof(sql), "SELECT DISTINCT `ZoneId`, `Name` FROM `PluginsConfig` WHERE `MonitorId` = %d AND `pluginName` IN (%s) AND `Name` IN ('RequireNatDet', 'IncludeNatDet', 'ReInitNatDet') AND `Value` = 'yes' ORDER BY `ZoneId` ASC;", m_nMonitorId, sLoadedPlugins.c_str());
    if (mysql_query(&dbconn, sql))
    {
        Error("Can't run query: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }
    MYSQL_RES *result = mysql_store_result(&dbconn);
    if (!result)
    {
        Error("Can't use query result: %s", mysql_error(&dbconn));
        exit(mysql_errno(&dbconn));
    }
    if (mysql_num_rows(result) > 0)
    {
        std::vector<zIdName> vSettings;
        for (unsigned int i = 0; MYSQL_ROW dbrow = mysql_fetch_row(result); i++)
        {
            if (mysql_errno(&dbconn))
            {
                Error("Can't fetch row: %s", mysql_error(&dbconn));
                mysql_free_result(result);
                exit(mysql_errno(&dbconn));
            }
            zIdName setting;
            setting.zoneId = (unsigned int)strtoul(dbrow[0], NULL, 0);
            setting.name = dbrow[1];
            vSettings.push_back(setting);
        }

        // Iterate over the zones and add the index to the vector if the zone id is found
        for (size_t i = 0; i != m_vMonitorZones.size(); i++)
        {
            zConf zoneConf;
            for (std::vector<zIdName>::iterator it = vSettings.begin(); it != vSettings.end(); it++)
            {
                if (it->zoneId == m_vMonitorZones[i].id)
                {
                    if (it->name == "RequireNatDet")
                        zoneConf.RequireNatDet = true;
                    else if (it->name == "IncludeNatDet")
                        zoneConf.IncludeNatDet = true;
                    else if (it->name == "ReInitNatDet")
                        zoneConf.ReInitNatDet = true;
                }
            }
            m_vZonesConfig.push_back(zoneConf);
        }
    }
    mysql_free_result(result);

    return true;
}


/*!\fn ImageAnalyser::getZoneConfig(int nZone, zConf& zoneConf)
 * \param nZone is the zone index (not the id in sql database)
 * \param zoneConf is a structure filled with the plugin settings of nZone
 */
bool ImageAnalyser::getZoneConfig(unsigned int nZone, zConf& zoneConf)
{
    if (nZone < m_vZonesConfig.size())
        zoneConf = m_vZonesConfig[nZone];
    else
        return false;
    return true;
}


/*!\fn ImageAnalyser::getRegPluginGenConf(std::string sPluginName, pGenConf& regPluginGenConf)
 * \param sPluginName is the name of the plugin (filename without extension)
 * \param regPluginGenConf is a structure filled with the general settings of the plugin
 * \return false if no setting is found
 */
bool ImageAnalyser::getRegPluginGenConf(std::string sPluginName, pGenConf& regPluginGenConf)
{
    std::map<std::string,pGenConf>::iterator it = mapRegPluginGenConf.find( sPluginName );
    if ( it == mapRegPluginGenConf.end() )
        return false;
    regPluginGenConf = it->second;
    return true;
}


/*!\fn ImageAnalyser::getRegPluginZoneConf(std::string sPluginName, PluginZoneConf& regPluginZoneConf)
 * \param sPluginName is the name of the plugin (filename without extension)
 * \param regPluginZoneConf is a map filled with the zone settings of the plugin
 */
void ImageAnalyser::getRegPluginZoneConf(std::string sPluginName, PluginZoneConf& regPluginZoneConf)
{
    std::map<std::string,PluginZoneConf>::iterator it = mapRegPluginZoneConf.find( sPluginName );

    if ( it != mapRegPluginZoneConf.end() )
        regPluginZoneConf = it->second;

    pZoneConf empty;

    for (size_t i = 0; i != m_vMonitorZones.size(); i++)
    {
        PluginZoneConf::iterator it2 = regPluginZoneConf.find( m_vMonitorZones[i].id );
         if ( it2 == regPluginZoneConf.end() )
             regPluginZoneConf[m_vMonitorZones[i].id] = empty;
    }
}

void ImageAnalyser::cleanupPlugins()
{

    std::string sPluginsToKeep;
    std::string sRequest;
    static char sql[ZM_SQL_MED_BUFSIZ];

    for ( DetectorsList::iterator It = m_Detectors.begin(); It != m_Detectors.end(); ++It )
    {
        if ( sPluginsToKeep.length() )
            sPluginsToKeep += ", ";
        sPluginsToKeep += "'" + (*It)->getPluginName() + "'";
    }

    if ( sPluginsToKeep.length() )
        sRequest = " AND `pluginName` NOT IN (" + sPluginsToKeep + ")";

    snprintf(sql, sizeof(sql), "DELETE FROM `PluginsConfig` WHERE `MonitorId` = %d%s;", m_nMonitorId, sRequest.c_str());

    if ( mysql_query( &dbconn, sql ) )
    {
        Error( "Can't delete plugin: %s", mysql_error( &dbconn ) );
        exit( mysql_errno( &dbconn ) );
    }
}
