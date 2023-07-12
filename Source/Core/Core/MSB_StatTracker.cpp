
#include "Core/MSB_StatTracker.h"

#include <iomanip>
#include <fstream>
#include <ctime>

//For LocalPLayers
#include "Common/CommonPaths.h"
#include "Common/IniFile.h"
#include "Core/LocalPlayersConfig.h"
#include "Common/Version.h"

#include "Common/Swap.h"

// Package for rendering info on screen
#include "VideoCommon/OnScreenDisplay.h"
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <iostream>
#include "Config/MainSettings.h"

#include "Common/TagSet.h"

void StatTracker::Run(){
    lookForTriggerEvents();
}

void StatTracker::lookForTriggerEvents(){
    // if (m_game_state != m_game_state_prev) {
    //     state_logger.writeToFile(c_game_state[m_game_state]);
    //     m_game_state_prev = m_game_state;
    // }

    if (m_event_state != m_event_state_prev) {
        // Write state on every change
        //state_logger.writeToFile(c_event_state[m_event_state]);
        if (m_game_info.currentEventVld()){
            // Add state of current event to event.history for logging purposes
            m_game_info.getCurrentEvent().history.push_back(m_event_state);

            if (m_event_state == EVENT_STATE::FINAL_RESULT) {
            // Write state details on play over
                state_logger.writeToFile(fmt::format(
                    "Game State: {}\n"
                    "Event State: {}\n"
                    "Event Num: {}\n"
                    "Inning: {}\n"
                    "Half Inning: {}\n"
                    "Batter: {}\n"
                    "Pitcher: {}\n"
                    "Event History: \n{}\n",
                    c_game_state[m_game_state],
                    c_event_state[m_event_state],
                    m_game_info.getCurrentEvent().event_num,
                    m_game_info.getCurrentEvent().inning,
                    m_game_info.getCurrentEvent().half_inning,
                    (m_game_info.getCurrentEvent().runner_batter) ? cCharIdToCharName.at(m_game_info.getCurrentEvent().runner_batter->char_id) : "None",
                    (m_game_info.getCurrentEvent().pitch) ? cCharIdToCharName.at(m_game_info.getCurrentEvent().pitch->pitcher_char_id) : "Pitch Not Thrown Yet",
                    m_game_info.getCurrentEvent().stringifyHistory()
                ));
            
                            
                if (m_game_info.getCurrentEvent().result_of_atbat != 0) {
                    u8 half_inning = m_game_info.getCurrentEvent().half_inning;

                    u8 batter_char_id = m_game_info.character_summaries[half_inning][m_game_info.getCurrentEvent().batter_roster_loc].char_id;
                    u8 pitcher_char_id = m_game_info.character_summaries[!half_inning][m_game_info.getCurrentEvent().pitcher_roster_loc].char_id;

                    std::string batter_name = cCharIdToCharName.at(batter_char_id);
                    std::string pitcher_name = cCharIdToCharName.at(pitcher_char_id);

                    if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
                    {
                      OSD::AddTypedMessage(
                          OSD::MessageType::GameStatePreviousPlayResult,
                          fmt::format("====PREVIOUS EVENT RESULT====\n"
                                      "Result of At Bat: {}\n"
                                      "RBI: {}\n"
                                      "Outs: {}\n"
                                      "Pitcher: {}\n"
                                      "Batter: {}\n",
                                      m_game_info.getCurrentEvent().result_of_atbat,
                                      m_game_info.getCurrentEvent().rbi,
                                      m_game_info.getCurrentEvent().outs, pitcher_name,
                                      batter_name),
                          10000, OSD::Color::RED);

                      OSD::AddTypedMessage(
                          OSD::MessageType::GameStatePreviousPlayInfo,
                          fmt::format(
                              "====PREVIOUS EVENT SUMMARY====\n"
                              "Event Num: {}\n"
                              "Inning: {}\n"
                              "Half Inning: {}\n"
                              "Batter: {}\n"
                              "Pitcher: {}\n"
                              "Event History: \n{}\n",
                              m_game_info.getCurrentEvent().event_num,
                              m_game_info.getCurrentEvent().inning,
                              m_game_info.getCurrentEvent().half_inning,
                              (m_game_info.getCurrentEvent().runner_batter) ?
                                  cCharIdToCharName.at(
                                      m_game_info.getCurrentEvent().runner_batter->char_id) :
                                  "None",
                              (m_game_info.getCurrentEvent().pitch) ?
                                  cCharIdToCharName.at(
                                      m_game_info.getCurrentEvent().pitch->pitcher_char_id) :
                                  "Pitch Not Thrown Yet",
                              m_game_info.getCurrentEvent().stringifyHistory()),
                          10000, OSD::Color::BLUE);
                    }
                };
            }
        }
        // Update previous event state variable for checking purposes
        m_event_state_prev = m_event_state;
    }

    if (m_game_state == GAME_STATE::INGAME) {
        if (m_game_info.currentEventVld()){
          if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
          {
            OSD::AddTypedMessage(
                OSD::MessageType::GameStateInfo,
                fmt::format(
                    "====CURRENT EVENT SUMMARY====\n"
                    "Game State: {}\n"
                    "Event State: {}\n"
                    "Event Num: {}\n"
                    "Inning: {}\n"
                    "Half Inning: {}\n"
                    "Batter: {}\n"
                    "Pitcher: {}\n"
                    "Event History: \n{}\n",
                    c_game_state[m_game_state], c_event_state[m_event_state],
                    m_game_info.getCurrentEvent().event_num, m_game_info.getCurrentEvent().inning,
                    m_game_info.getCurrentEvent().half_inning,
                    (m_game_info.getCurrentEvent().runner_batter) ?
                        cCharIdToCharName.at(m_game_info.getCurrentEvent().runner_batter->char_id) :
                        "None",
                    (m_game_info.getCurrentEvent().pitch) ?
                        cCharIdToCharName.at(m_game_info.getCurrentEvent().pitch->pitcher_char_id) :
                        "Pitch Not Thrown Yet",
                    m_game_info.getCurrentEvent().stringifyHistory()),
                3000, OSD::Color::CYAN);
          }
        }
    } else {
      if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
      {
        OSD::AddTypedMessage(OSD::MessageType::GameStateInfo, fmt::format(
            "Game State: {}\n"
            "Event State: {}\n",
            c_game_state[m_game_state],
            c_event_state[m_event_state]            
        ), 200, OSD::Color::CYAN);
        }
    }

    //At Bat State Machine
    if (m_game_state == GAME_STATE::INGAME){
        switch(m_event_state){
            case (EVENT_STATE::INIT_EVENT):
                //Create new event, collect runner data

                //Capture the rising edge of the AtBat Scene
                if (PowerPC::HostRead_U8(aGameControlStateCurr) == 0x1 && PowerPC::HostRead_U8(aGameControlStatePrev) != 0x1){

                    m_game_info.events[m_game_info.event_num] = Event();
                    m_game_info.getCurrentEvent().event_num = m_game_info.event_num;

                    logEventState(m_game_info.getCurrentEvent());
                    logGameInfo();

                    //Get users and captains
                    //POST OngoingGame
                    if (m_game_info.init_game == true) {
                        m_game_info.init_game = false;
                        initPlayerInfo();
                    }

                    m_game_info.getCurrentEvent().runner_batter = logRunnerInfo(0);
                    m_game_info.getCurrentEvent().runner_1 = logRunnerInfo(1);
                    m_game_info.getCurrentEvent().runner_2 = logRunnerInfo(2);
                    m_game_info.getCurrentEvent().runner_3 = logRunnerInfo(3);

                    if (!m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].initialized){
                        std::cout << " Initializing fielders for team: " << std::to_string(!m_game_info.getCurrentEvent().half_inning) << "\n";
                        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].initTracker(!m_game_info.getCurrentEvent().half_inning);
                    }

                    m_event_state = EVENT_STATE::WAITING_FOR_EVENT;

                    std::cout << "Init event " << std::to_string(m_game_info.event_num) << "\n";
                }
                else if (PowerPC::HostRead_U32(aGameId) == 0){
                    onGameQuit();

                    //Remove current event, wasn't finished
                    auto it = m_game_info.events.find(m_game_info.event_num);
                    m_game_info.events.erase(it);

                    m_event_state = EVENT_STATE::GAME_OVER;
                }
                break;
            //Look for Pitch
            case (EVENT_STATE::WAITING_FOR_EVENT):
                //Handle quit to main menu
                if (PowerPC::HostRead_U32(aGameId) == 0){
                    onGameQuit();

                    //Remove current event, wasn't finished
                    auto it = m_game_info.events.find(m_game_info.event_num);
                    m_game_info.events.erase(it);

                    m_event_state = EVENT_STATE::GAME_OVER;
                    break;
                }

                //Update OngoingGame
                if (!m_game_info.post_ongoing_game && m_game_info.update_ongoing_game){
                    updateOngoingGame(m_game_info.getCurrentEvent());
                    m_game_info.update_ongoing_game = false;
                }

                //Trigger Events to look for
                //1. Are runners stealing and pitcher stepped off the mound
                //2. Has pitch started?
                //3. Has game been paused, reinit 
                if (PowerPC::HostRead_U8(aGameControlStateCurr) == 0xb){
                    std::cout << "Game paused, need to re-init event " << std::to_string(m_game_info.event_num) << "\n";
                    logGameInfo();
                    updateOngoingGame(m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::INIT_EVENT;
                }
                //Watch for Runners Stealing
                if (PowerPC::HostRead_U8(aAB_PitchThrown) || PowerPC::HostRead_U8(aAB_PickoffAttempt)){
                    //If HUD not produced for this event, produce HUD JSON
                    logGameInfo();

                    if (m_game_info.getCurrentEvent().write_hud_ab.first) {
                        std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                        std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "a", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                        File::Delete(hud_file_path);
                        File::WriteStringToFile(hud_file_path, json);
                        //No longer need to write HUD B
                        m_game_info.getCurrentEvent().write_hud_ab.first = false;
                    }

                    if(PowerPC::HostRead_U8(aAB_PitchThrown)){
                        std::cout << "Pitch detected!\n";

                        //Check for fielder swaps
                        std::cout << " Evaluating fielders for team: " << std::to_string(!m_game_info.getCurrentEvent().half_inning) << "\n";
                        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].evaluateFielders();

                        m_game_info.getCurrentEvent().pitch = std::make_optional(Pitch());

                        //Check if pitcher was at center of mound, if so this is a potential DB
                        if (PowerPC::HostRead_U8(aFielder_Pos_X) == 0){
                            m_game_info.getCurrentEvent().pitch->potential_db = true;
                            std::cout << "Potential DB!\n";
                        }

                        //Pitch has started
                        m_event_state = EVENT_STATE::PITCH_RESULT;
                    }
                    else if(PowerPC::HostRead_U8(aAB_PickoffAttempt)) {
                        std::cout << "Pick of attempt detected!\n";
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                        m_game_info.getCurrentEvent().pick_off_attempt = true;
                    }
                }
                break;
            case (EVENT_STATE::PITCH_RESULT): //Look for contact or end of pitch

                // === Monitor ===
                //DBs
                //If the pitcher started in the center of the mound this is a potential DB
                //If the ball curves at any point it is no longer a DB
                if (m_game_info.getCurrentEvent().pitch->potential_db && (PowerPC::HostRead_U8(aAB_PitcherHasCtrlofPitch) == 1)) {
                    if (floatConverter(PowerPC::HostRead_U32(aAB_PitchCurveInput)) != 0) {
                        std::cout << "No longer potential DB!\n";
                        m_game_info.getCurrentEvent().pitch->potential_db = false;
                    }
                }
                //While pitch is in flight, record runner activity 
                //Log if runners are stealing
                if (m_game_info.getCurrentEvent().runner_1) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_1.value());
                }
                if (m_game_info.getCurrentEvent().runner_2) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_2.value());
                }
                if (m_game_info.getCurrentEvent().runner_3) {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_3.value());
                }

                // === Transition ===

                //Conditions to leave the state: Contact, Ball beyond batter, HBP
                //Contact
                if (PowerPC::HostRead_U8(aAB_ContactMade)){
                    logPitch(m_game_info.getCurrentEvent());
                    logContact(m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::CONTACT_RESULT;
                }
                //If the ball gets behind the batter while mid pitch OR play flag is false (safety incase we miss the first cond), record miss
                else if (PowerPC::HostRead_U8(aAB_MissedBall)){
                    logPitch(m_game_info.getCurrentEvent());
                    m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                }
                else if (PowerPC::HostRead_U8(aAB_HitByPitch) == 1){
                    //Log HBP
                    logPitch(m_game_info.getCurrentEvent());
                    if (!PowerPC::HostRead_U8(aAB_PitchThrown)) {
                        m_game_info.getCurrentEvent().result_of_atbat = PowerPC::HostRead_U8(aAB_FinalResult);
                        m_event_state = EVENT_STATE::PLAY_OVER;
                    }
                }

                break;
            case (EVENT_STATE::CONTACT_RESULT):                
                if (PowerPC::HostRead_U8(aAB_ContactResult) != 0){
                    //Indicate that pitch resulted in contact and log contact details
                    m_game_info.getCurrentEvent().pitch->pitch_result = 6;
                    logContactResult(&m_game_info.getCurrentEvent().pitch->contact.value()); //Land vs Caught vs Foul, Landing POS.
                    if(m_event_state != EVENT_STATE::LOG_FIELDER) { //If we don't need to scan for which fielder fields the ball
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                    break;
                }

                // === Monitor === 
                //Log ball pos.
                //Ball is still in air
                else{
                    Contact* contact = &m_game_info.getCurrentEvent().pitch->contact.value();
                    //Final Result Ball
                    contact->ball_x_pos.read_value();
                    contact->ball_y_pos.read_value();
                    contact->ball_z_pos.read_value();
                }
                //Could bobble before the ball hits the ground.
                //Search for bobble if we haven't recorded one yet and the ball hasn't been collected yet
                if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                     
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble();
                }

                break;
            case (EVENT_STATE::LOG_FIELDER):
                //Look for bobble if we haven't seen any fielder touch the ball yet
                if (!m_game_info.getCurrentEvent().pitch->contact->first_fielder.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                    
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->first_fielder = logFielderBobble();
                }
                
                if (!m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                    //Returns fielder that is holding the ball. Otherwise nullptr
                    m_game_info.getCurrentEvent().pitch->contact->collect_fielder = logFielderWithBall();
                    if (m_game_info.getCurrentEvent().pitch->contact->collect_fielder.has_value()){
                        //Start watching runners for outs when the ball has finally been collected
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                }

                //Break out if play ends without fielding the ball (HR or other play ending hit)
                if (!PowerPC::HostRead_U8(aAB_PitchThrown)) {
                    m_game_info.getCurrentEvent().result_of_atbat = PowerPC::HostRead_U8(aAB_FinalResult);
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                break;
            case (EVENT_STATE::MONITOR_RUNNERS):
                if (!PowerPC::HostRead_U8(aAB_PitchThrown) && !PowerPC::HostRead_U8(aAB_PickoffAttempt)){
                    m_game_info.getCurrentEvent().result_of_atbat = PowerPC::HostRead_U8(aAB_FinalResult);
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                else {
                    logRunnerEvents(&m_game_info.getCurrentEvent().runner_batter.value());
                    if (m_game_info.getCurrentEvent().runner_1) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_1.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_2) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_2.value());
                    }
                    if (m_game_info.getCurrentEvent().runner_3) {
                        logRunnerEvents(&m_game_info.getCurrentEvent().runner_3.value());
                    }
                }
                break;
            case (EVENT_STATE::PLAY_OVER):
                if (!PowerPC::HostRead_U8(aAB_PitchThrown)){
                    m_game_info.getCurrentEvent().rbi = PowerPC::HostRead_U8(aAB_RBI);

                    //runner_batter out, contact_secondary
                    logFinalResults(m_game_info.getCurrentEvent());

                    //Determine if this was pitch was a DB
                    if (m_game_info.getCurrentEvent().pitch->potential_db){
                        m_game_info.getCurrentEvent().pitch->db = 1;                    
                        std::cout << "Logging DB!\n";
                    }

                    // Clear result of AB for pickoffs
                    if (m_game_info.getCurrentEvent().pick_off_attempt) {
                        m_game_info.getCurrentEvent().result_of_atbat = 0;
                    }

                    m_event_state = EVENT_STATE::FINAL_RESULT;
                    std::cout << "Play over\n";
                }
                break;
            case (EVENT_STATE::FINAL_RESULT):
                //Log post event HUD to file
                if (m_game_info.getCurrentEvent().write_hud_ab.second){

                    //Fill in current state for HUD
                    logGameInfo();

                    if (m_game_info.post_ongoing_game == true) {
                        m_game_info.post_ongoing_game = false;
                        postOngoingGame(m_game_info.getCurrentEvent());
                    }

                    //Store current state as previous state
                    m_game_info.previous_state = m_game_info.getCurrentEvent();

                    std::string hud_file_path = File::GetUserPath(D_HUDFILES_IDX) + "decoded.hud.json";
                    std::string json = getHUDJSON(std::to_string(m_game_info.event_num) + "b", m_game_info.getCurrentEvent(), m_game_info.previous_state, true);
                    File::Delete(hud_file_path);
                    File::WriteStringToFile(hud_file_path, json);

                    //No longer need to write HUD B
                    m_game_info.getCurrentEvent().write_hud_ab.second = false;
                }

                // === Transitions ===

                if (PowerPC::HostRead_U8(aGameControlStateCurr) == 0x7){
                    //Increment event count
                    ++m_game_info.event_num;
                    //Save position as prev position
                    u8 fielding_team_id = (m_game_info.previous_state.value().half_inning == 1) ? 0 : 1;
                    m_fielder_tracker[fielding_team_id].incrementBattersForPosition();
                    m_fielder_tracker[fielding_team_id].newBatter();
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    m_game_info.update_ongoing_game = true;
                    std::cout << "Logging Final Result\n" << "Starting next AB\n\n";
                }
                else if (PowerPC::HostRead_U8(aGameControlStateCurr) == 0x1 && !m_game_info.previous_state.value().pitch.has_value()){
                    //Increment event count
                    ++m_game_info.event_num;
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    std::cout << "Logging Final Result\n" << "Pickoff over\n\n";
                }
                else if ((PowerPC::HostRead_U8(aGameControlStateCurr) == 0xE) || (PowerPC::HostRead_U8(aEndOfGameFlag) == 1)){ //MVP screen
                    //Increment event count
                    m_event_state = EVENT_STATE::GAME_OVER;
                    std::cout << "Logging Final Result\n" << "Game Over\n\n";
                }
                else if ((m_game_info.previous_state.value().balls < 4 || m_game_info.previous_state.value().strikes < 3) && m_game_info.getCurrentEvent().result_of_atbat == 0) {
                    ++m_game_info.event_num;
                    m_event_state = EVENT_STATE::INIT_EVENT;
                    std::cout << "Logging Final Result\n" << "Starting next pitch of AB\n\n";
                }
                break;
            case (EVENT_STATE::GAME_OVER):
                std::cout << "Game Over. Waiting for next game\n";
                break;
            case (EVENT_STATE::UNDEFINED):
                std::cout << "UNDEFINED STATE\n";
                m_event_state = EVENT_STATE::INIT_EVENT;
                break;                
            default:
                std::cout << "Unknown Event State\n";
                m_event_state = EVENT_STATE::INIT_EVENT;
                break;
        }
    }

    //Game State Machine
    switch (m_game_state){
        case (GAME_STATE::PREGAME):
            //Start recording when GameId is set AND record button is pressed AND game has started
            //std::cout << std::hex << "GameId=" << PowerPC::HostRead_U32(aGameId) << "GameState=" <<  PowerPC::HostRead_U8(aGameControlStateCurr) << '\n';
            if ((PowerPC::HostRead_U32(aGameId) != 0) && (PowerPC::HostRead_U8(aGameControlStateCurr) == 0x5) ) {
                m_game_info.game_id = PowerPC::HostRead_U32(aGameId);
                //Sample settings
                m_game_info.netplay = m_state.m_netplay_session;
                m_game_info.host    = m_state.m_is_host;
                m_game_info.netplay_opponent_alias = m_state.m_netplay_opponent_alias;
                m_game_info.tag_set_id =
                    m_state.m_netplay_session ? m_state.tag_set_id_netplay : m_state.tag_set_id_local;


                m_game_state = GAME_STATE::INGAME;

                std::string tag_set_id_str = "\"\"";
                if (m_game_info.tag_set_id.has_value()){
                    tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
                }
                std::cout << "PREGAME->INGAME (GameID=" << std::to_string(m_game_info.game_id) << ", TagSetID=" << tag_set_id_str <<")\n";
                std::cout << "                (Netplay=" << m_game_info.netplay << ", Host=" << m_game_info.host << ")\n";
            }
            break;
        case (GAME_STATE::INGAME):
            if (m_event_state == EVENT_STATE::GAME_OVER){
                logGameInfo();
                std::cout << "Logging Character Stats\n";

                std::string jsonPath = getStatJsonPath("decoded.");
                std::string json = getStatJSON(true);
                    
                File::WriteStringToFile(jsonPath, json);

                jsonPath = getStatJsonPath("");
                json = getStatJSON(false, true);
                //TODO: See if user has signed up for beta test features in future
                File::WriteStringToFile(jsonPath, json);

                //File::WriteStringToFile(jsonPath, json);
                //https://api.projectrio.app/populate_db

                //Print server warning message
                OSD::AddTypedMessage(OSD::MessageType::GameStateInfo, fmt::format(
                    "Submitting game to server \n",
                    "DO NOT LEAVE THE GAME OR CLOSE RIO"           
                ), 500, OSD::Color::RED);
                
                json = getStatJSON(false, false);
                if (shouldSubmitGame()) {
                    const Common::HttpRequest::Response response =
                    m_http.Post("https://api.projectrio.app/populate_db/", json,
                        {
                            {"Content-Type", "application/json"},
                        }
                    );

                    // Print server warning message
                    OSD::AddTypedMessage(OSD::MessageType::GameStateInfo,
                                         fmt::format("Done submitting game \n", "SAFE TO QUIT"),
                                         5000, OSD::Color::GREEN);
                }

                std::cout << "Logging to " << jsonPath << "\n";
                std::cout << "INGAME->ENDGAME\n";


                m_game_state = GAME_STATE::ENDGAME_LOGGED;
            }
            break;
        case (GAME_STATE::ENDGAME_LOGGED):
            init();

            std::cout << "ENDGAME->PREGAME\n";
            break;
        case (GAME_STATE::UNDEFINED):
            std::cout << "UNDEFINED GAME STATE\n";
            m_event_state = EVENT_STATE::INIT_EVENT;
            break;
        default:
            std::cout << "Unknown Game State\n";
            m_event_state = EVENT_STATE::INIT_EVENT;
            break;        
    }
}

void StatTracker::logGameInfo(){

    std::time_t unix_time = std::time(nullptr);

    m_game_info.end_unix_date_time = std::to_string(unix_time);
    m_game_info.end_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.end_local_date_time.pop_back();

    m_game_info.stadium = PowerPC::HostRead_U8(aStadiumId);

    m_game_info.innings_selected = PowerPC::HostRead_U8(aInningsSelected);
    m_game_info.innings_played   = PowerPC::HostRead_U8(aAB_Inning);

    ////Captains
    //if (m_game_info.away_port == m_game_info.team0_port){
    //    m_game_info.away_captain = PowerPC::HostRead_U8(aTeam0_Captain);
    //    m_game_info.home_captain = PowerPC::HostRead_U8(aTeam1_Captain);
    //}
    //else{
    //    m_game_info.away_captain = PowerPC::HostRead_U8(aTeam1_Captain);
    //    m_game_info.home_captain = PowerPC::HostRead_U8(aTeam0_Captain);
    //}

    m_game_info.away_score = PowerPC::HostRead_U16(aAwayTeam_Score);
    m_game_info.home_score = PowerPC::HostRead_U16(aHomeTeam_Score);

    for (int team=0; team < cNumOfTeams; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){
            logDefensiveStats(team, roster);
            logOffensiveStats(team, roster);
        }
    }
}

void StatTracker::logDefensiveStats(int in_team_id, int roster_id){
    u32 offset = (in_team_id * cRosterSize * c_defensive_stat_offset) + (roster_id * c_defensive_stat_offset);

    u32 ingame_attribute_table_offset = (in_team_id * cRosterSize * c_roster_table_offset) + (roster_id * c_roster_table_offset);
    u32 is_starred_offset = (in_team_id * cRosterSize) + roster_id;

    u8 team_id_port = (in_team_id == 0) ? m_game_info.team0_port : m_game_info.team1_port;
    u8 idx = (team_id_port == m_game_info.home_port);
    
    auto& stat = m_game_info.character_summaries[idx][roster_id].end_game_defensive_stats;

    m_game_info.character_summaries[idx][roster_id].is_starred = PowerPC::HostRead_U8(aPitcher_IsStarred + is_starred_offset);

    stat.batters_faced       = PowerPC::HostRead_U8(aPitcher_BattersFaced + offset);
    stat.runs_allowed        = PowerPC::HostRead_U16(aPitcher_RunsAllowed + offset);
    stat.earned_runs         = PowerPC::HostRead_U16(aPitcher_RunsAllowed + offset);
    stat.batters_walked      = PowerPC::HostRead_U16(aPitcher_BattersWalked + offset);
    stat.batters_hit         = PowerPC::HostRead_U16(aPitcher_BattersHit + offset);
    stat.hits_allowed        = PowerPC::HostRead_U16(aPitcher_HitsAllowed + offset);
    stat.homeruns_allowed    = PowerPC::HostRead_U16(aPitcher_HRsAllowed + offset);
    stat.pitches_thrown      = PowerPC::HostRead_U16(aPitcher_PitchesThrown + offset);
    stat.stamina             = PowerPC::HostRead_U16(aPitcher_Stamina + offset);
    stat.was_pitcher         = PowerPC::HostRead_U8(aPitcher_WasPitcher + offset);
    stat.batter_outs         = PowerPC::HostRead_U8(aPitcher_BatterOuts + offset);
    stat.outs_pitched        = PowerPC::HostRead_U8(aPitcher_OutsPitched + offset);
    stat.strike_outs         = PowerPC::HostRead_U8(aPitcher_StrikeOuts + offset);
    stat.star_pitches_thrown = PowerPC::HostRead_U8(aPitcher_StarPitchesThrown + offset);

    //Get inherent values. Doesn't strictly belong here but we need the adjusted_team_id
    m_game_info.character_summaries[idx][roster_id].char_id = PowerPC::HostRead_U8(aInGame_CharAttributes_CharId + ingame_attribute_table_offset);
    m_game_info.character_summaries[idx][roster_id].fielding_hand = PowerPC::HostRead_U8(aInGame_CharAttributes_FieldingHand + ingame_attribute_table_offset);
    m_game_info.character_summaries[idx][roster_id].batting_hand = PowerPC::HostRead_U8(aInGame_CharAttributes_BattingHand + ingame_attribute_table_offset);

}

void StatTracker::logOffensiveStats(int in_team_id, int roster_id){
    u32 offset = ((in_team_id * cRosterSize * c_offensive_stat_offset)) + (roster_id * c_offensive_stat_offset);

    u8 team_id_port = (in_team_id == 0) ? m_game_info.team0_port : m_game_info.team1_port;
    u8 idx = (team_id_port == m_game_info.home_port);

    auto& stat = m_game_info.character_summaries[idx][roster_id].end_game_offensive_stats;

    stat.at_bats          = PowerPC::HostRead_U8(aBatter_AtBats + offset);
    stat.hits             = PowerPC::HostRead_U8(aBatter_Hits + offset);
    stat.singles          = PowerPC::HostRead_U8(aBatter_Singles + offset);
    stat.doubles          = PowerPC::HostRead_U8(aBatter_Doubles + offset);
    stat.triples          = PowerPC::HostRead_U8(aBatter_Triples + offset);
    stat.homeruns         = PowerPC::HostRead_U8(aBatter_Homeruns + offset);
    stat.successful_bunts = PowerPC::HostRead_U8(aBatter_BuntSuccess + offset);
    stat.sac_flys         = PowerPC::HostRead_U8(aBatter_SacFlys + offset);
    stat.strikouts        = PowerPC::HostRead_U8(aBatter_Strikeouts + offset);
    stat.walks_4balls     = PowerPC::HostRead_U8(aBatter_Walks_4Balls + offset);
    stat.walks_hit        = PowerPC::HostRead_U8(aBatter_Walks_Hit + offset);
    stat.rbi              = PowerPC::HostRead_U8(aBatter_RBI + offset);
    stat.bases_stolen     = PowerPC::HostRead_U8(aBatter_BasesStolen + offset);
    stat.star_hits        = PowerPC::HostRead_U8(aBatter_StarHits + offset);

    m_game_info.character_summaries[idx][roster_id].end_game_defensive_stats.big_plays = PowerPC::HostRead_U8(aBatter_BigPlays + offset);
}

void StatTracker::logEventState(Event& in_event){
    in_event.inning          = PowerPC::HostRead_U8(aAB_Inning);
    in_event.half_inning     = PowerPC::HostRead_U8(aAB_HalfInning);

    //Figure out scores
    in_event.away_score = PowerPC::HostRead_U16(aAwayTeam_Score);
    in_event.home_score = PowerPC::HostRead_U16(aHomeTeam_Score);

    in_event.balls           = PowerPC::HostRead_U8(aAB_Balls);
    in_event.strikes         = PowerPC::HostRead_U8(aAB_Strikes);
    in_event.outs            = PowerPC::HostRead_U8(aAB_Outs);
    
    //Figure out star ownership
    if (m_game_info.team0_port == m_game_info.away_port){
        in_event.away_stars = PowerPC::HostRead_U8(aAB_P1_Stars);
        in_event.home_stars = PowerPC::HostRead_U8(aAB_P2_Stars);
    }
    else {
        in_event.away_stars = PowerPC::HostRead_U8(aAB_P2_Stars);
        in_event.home_stars = PowerPC::HostRead_U8(aAB_P1_Stars);
    }
    
    in_event.is_star_chance  = PowerPC::HostRead_U8(aAB_IsStarChance);
    in_event.chem_links_ob   = PowerPC::HostRead_U8(aAB_ChemLinksOnBase);

    //The following stamina lookup requires team_id to be in teams of team0 or team1

    auto batter_fielder_ports = getBatterFielderPorts();
    u8 pitching_team = (batter_fielder_ports.second == m_game_info.team1_port); //1 if the pitching team is team1
    u8 pitcher_roster_loc = PowerPC::HostRead_U8(aAB_PitcherRosterID);
    
    //Calc the pitcher stamina offset and add it to the base stamina addr - TODO move to EventSummary
    u32 pitcherStaminaOffset = ((pitching_team * cRosterSize * c_defensive_stat_offset) + (pitcher_roster_loc * c_defensive_stat_offset));
    in_event.pitcher_stamina = PowerPC::HostRead_U16(aPitcher_Stamina + pitcherStaminaOffset);

    in_event.pitcher_roster_loc = PowerPC::HostRead_U8(aAB_PitcherRosterID);
    in_event.batter_roster_loc  = PowerPC::HostRead_U8(aAB_BatterRosterID);
    in_event.catcher_roster_loc = PowerPC::HostRead_U8(aFielder_RosterLoc + (1 * cFielder_Offset));
}

void StatTracker::logContact(Event& in_event){
    std::cout << "Logging Contact\n";

    Pitch* pitch = &in_event.pitch.value();
    //Create contact object to populate and get a ptr to it
    pitch->contact = std::make_optional(Contact());
    std::cout << "  Pitch Type: " << std::to_string(in_event.pitch->pitch_type) << "\n";
    Contact* contact = &in_event.pitch->contact.value();

    contact->power.read_value();
    contact->vert_angle.read_value();
    contact->horiz_angle.read_value();
    contact->ball_x_velo.read_value();
    contact->ball_y_velo.read_value();
    contact->ball_z_velo.read_value();
    contact->ball_contact_x_pos.read_value();
    contact->ball_contact_z_pos.read_value();
    contact->contact_absolute.read_value();
    contact->contact_quality.read_value();
    contact->rng1.read_value();
    contact->rng2.read_value();
    contact->rng3.read_value();
    contact->type_of_contact.read_value();
    contact->moon_shot.read_value();
    contact->charge_power_up.read_value();
    contact->charge_power_down.read_value();
    contact->input_direction_push_pull.read_value();
    contact->frame_of_swing.read_value();

    //More ball flight info
    contact->ball_max_height.read_value();
    contact->ball_hang_time.read_value();

    u32 aStickInput = aAB_ControlStickInput + (getBatterFielderPorts().first * cControl_Offset);
    //std::cout << "Batter Port=" << std::to_string(getBatterFielderPorts().first) << " Stick Addr=" << std::hex << aStickInput << " Stick Value=" << (PowerPC::HostRead_U16(aStickInput) & 0xF) << "\n";
    contact->input_direction_stick.set_value(PowerPC::HostRead_U16(aStickInput) & 0xF); //Mask off the lower 4 bits which are the control stick directions
    //std::cout << "  Stick Value Decoded=" << decode("StickVec", contact->input_direction_stick.get_value(), true) << "\n";
    std::cout << "SWING: " << contact->frame_of_swing.get_key_value_string().first << "=" << contact->frame_of_swing.get_key_value_string().second << "\n";
    std::cout << "\n";
}

void StatTracker::logPitch(Event& in_event){
    std::cout << "Logging Pitching\n";

    in_event.pitch->logged = true;
    in_event.pitch->pitcher_team_id    = !in_event.half_inning;
    in_event.pitch->pitcher_char_id    = PowerPC::HostRead_U8(aAB_PitcherID);
    in_event.pitch->pitch_type         = PowerPC::HostRead_U8(aAB_PitchType);
    in_event.pitch->charge_type        = PowerPC::HostRead_U8(aAB_ChargePitchType);
    in_event.pitch->star_pitch         = ((PowerPC::HostRead_U8(aAB_StarPitch_NonCaptain) > 0) || (PowerPC::HostRead_U8(aAB_StarPitch_Captain) > 0));
    in_event.pitch->pitch_speed        = PowerPC::HostRead_U8(aAB_PitchSpeed);

    in_event.pitch->ball_z_strike_vs_ball = PowerPC::HostRead_U32(aAB_PitchBallPosZStrikezone);
    in_event.pitch->bat_contact_x_pos.read_value();
    in_event.pitch->bat_contact_z_pos.read_value();

    float ballposz_strikezone = floatConverter(in_event.pitch->ball_z_strike_vs_ball);
    float strikezone_left = floatConverter(PowerPC::HostRead_U32(aAB_PitchStrikezoneEdgeLeft));
    float strikezone_right = floatConverter(PowerPC::HostRead_U32(aAB_PitchStrikezoneEdgeRight));
    in_event.pitch->ball_in_strikezone = (strikezone_left < ballposz_strikezone && ballposz_strikezone < strikezone_right) ? 1 : 0;
    
    // === Batter info ===

    //First slap,charge,star,bunt
    u8 swing_type = PowerPC::HostRead_U8(aAB_TypeOfSwing); //0=Slap, 1=charge, 3=bunt
    u8 star_swing = PowerPC::HostRead_U8(aAB_StarSwing);
    u8 adjusted_swing = 0; //0=miss, 1=slap, 2=charge, 3=star, 4=bunt
    //Adjust swing to definition
    if (star_swing != 0){
        adjusted_swing = 3;
    }
    else {
        adjusted_swing = swing_type + 1;
    }

    //Use adjusted swing if swing and miss, else 0 (or 4 for bunt)
    u8 any_swing = PowerPC::HostRead_U8(aAB_AnySwing); //0=No swing, 1=swing
    if (any_swing == 0) {
        in_event.pitch->type_of_swing = 0;
    }
    else if (any_swing >= 1){
        in_event.pitch->type_of_swing = adjusted_swing;
    }

    std::cout << "SWING: Swing Type=" << std::to_string(swing_type) << " Star Swing=" << std::to_string(star_swing) 
              << " AnySwing=" << std::to_string(PowerPC::HostRead_U8(aAB_AnySwing)) << " Final=" << std::to_string(in_event.pitch->type_of_swing) << "\n";
}

void StatTracker::logContactResult(Contact* in_contact){
    std::cout << "Logging Contact Result\n";

    u8 result = PowerPC::HostRead_U8(aAB_ContactResult);

    //Log primary contact result (and secondary if possible)
    if (result == 1 || result == 2){
        in_contact->primary_contact_result = result+1; //Landed Fair
        m_event_state = EVENT_STATE::LOG_FIELDER;
        in_contact->ball_x_pos.read_value();
        in_contact->ball_y_pos.read_value();
        in_contact->ball_z_pos.read_value();

        //If 2, ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall();
    }
    else if (result == 3){
        in_contact->primary_contact_result = 0; //Out (secondary=caught)
        in_contact->secondary_contact_result = 0; //Out (secondary=caught)

        //If the ball is caught, use the balls position from the frame before to avoid the ball_pos
        //from matching the fielders
        in_contact->ball_x_pos.set_value_to_prev();
        in_contact->ball_y_pos.set_value_to_prev();
        in_contact->ball_z_pos.set_value_to_prev();

        //Ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        in_contact->collect_fielder = logFielderWithBall();

        //Increment outs for that position for fielder
        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(in_contact->collect_fielder->fielder_roster_loc, in_contact->collect_fielder->fielder_pos);
        //Indicate if fielder had been swapped for this batterid = 
        //fielding team is !half_inning
        in_contact->collect_fielder->fielder_swapped_for_batter = m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].wasFielderSwappedForBatter(in_contact->collect_fielder->fielder_roster_loc);

        std::cout << "Was fielder swapped. Team_id=" << std::to_string(!m_game_info.getCurrentEvent().half_inning) 
                  << " Fielder Roster=" << std::to_string(in_contact->collect_fielder->fielder_roster_loc)
                  << " Swapped=" << std::to_string(in_contact->collect_fielder->fielder_swapped_for_batter) << "\n";
    }
    else if (result == 0xFF){ // Known bug: this will be true for foul or HR. Correct when adjusting secondary contact later
        in_contact->primary_contact_result = 1; //Foul
        in_contact->secondary_contact_result = 3; //Foul
        in_contact->ball_x_pos.read_value();
        in_contact->ball_y_pos.read_value();
        in_contact->ball_z_pos.read_value();
    }
    else{
        in_contact->primary_contact_result = result;
        in_contact->secondary_contact_result = 0xFF; //???
        in_contact->ball_x_pos.read_value();
        in_contact->ball_y_pos.read_value();
        in_contact->ball_z_pos.read_value();
    }
}

void StatTracker::logFinalResults(Event& in_event){

    //Indicate strikeout in the runner_batter
    if (in_event.result_of_atbat == 1){
        //0x10 in runner_batter denotes strikeout
        in_event.runner_batter->out_type = 0x10;
    }

    if (in_event.pitch.has_value() && in_event.pitch->contact.has_value()){
        Contact* contact = &in_event.pitch->contact.value();
        //Fill in secondary result for contact
        if (in_event.result_of_atbat >= 0x7 && in_event.result_of_atbat <= 0xF){
            //0x10 in runner_batter denotes strikeout
            contact->secondary_contact_result = in_event.result_of_atbat;
            contact->primary_contact_result = 2; // Correct if set to foul
        }
        else if ((in_event.runner_batter->out_type == 2) || (in_event.runner_batter->out_type == 3)){
            contact->secondary_contact_result = in_event.runner_batter->out_type;
        }
        else if ((in_event.runner_batter->out_type == 0) && (in_event.result_of_atbat == 4)){
            contact->secondary_contact_result = in_event.result_of_atbat;
        }
    }

    //num_outs_during_play
    auto num_outs = in_event.num_outs_during_play.read_value();
    std::cout << "Num outs for play=" << std::to_string(num_outs) << "\n";
    m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementBatterOutForPosition(num_outs);

    //If the runner got out at first (forced), increment outs for the fielder who first touched the ball
    if (in_event.runner_batter->out_type == 2) {
        //First fielder to touch the ball
        Fielder* fielder;

        //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielde
        if (in_event.pitch->contact->first_fielder.has_value()) { fielder = &in_event.pitch->contact->first_fielder.value(); }
        else {fielder = &in_event.pitch->contact->collect_fielder.value();}

        m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].incrementOutForPosition(fielder->fielder_roster_loc, fielder->fielder_pos);
    }
}

std::string StatTracker::getStatJsonPath(std::string prefix){
    std::string away_player_name;
    std::string home_player_name;
    if (m_game_info.away_port == m_game_info.team0_port) {
        away_player_name = m_game_info.team0_player.GetUsername();
        home_player_name = m_game_info.team1_player.GetUsername();
    }
    else{
        away_player_name = m_game_info.team1_player.GetUsername();
        home_player_name = m_game_info.team0_player.GetUsername();
    }

    std::time_t unix_time = std::time(nullptr);
    char datetime_c[256];
    std::strftime(datetime_c, sizeof(datetime_c), "%Y%m%dT%H%M%S", std::localtime(&unix_time));
    
    std::string file_name = prefix + datetime_c + "_" + away_player_name 
                   + "-Vs-" + home_player_name
                   + "_" + std::to_string(m_game_info.game_id) + ".json";

    std::string full_file_path = File::GetUserPath(D_MSSBFILES_IDX) + file_name;

    return full_file_path;
}

std::string StatTracker::getStatJSON(bool inDecode, bool hide_riokey){
    //TODO switch to IDs when submitting game
    std::string away_player_info = (inDecode || hide_riokey) ? m_game_info.getAwayTeamPlayer().GetUsername() : m_game_info.getAwayTeamPlayer().GetUserID();
    std::string home_player_info = (inDecode || hide_riokey) ? m_game_info.getHomeTeamPlayer().GetUsername() : m_game_info.getHomeTeamPlayer().GetUserID();

    std::stringstream json_stream;

    json_stream << "{\n";
    std::string stadium = (inDecode) ? "\"" + cStadiumIdToStadiumName.at(m_game_info.stadium) + "\"" : std::to_string(m_game_info.stadium);
    std::string start_date_time = (inDecode) ? m_game_info.start_local_date_time : m_game_info.start_unix_date_time;
    std::string end_date_time = (inDecode) ? m_game_info.end_local_date_time : m_game_info.end_unix_date_time;
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Date - Start\": \"" << start_date_time << "\",\n";
    json_stream << "  \"Date - End\": \"" << end_date_time << "\",\n";
    
    std::string tag_set_id_str = "\"\"";
    if (m_game_info.tag_set_id.has_value()){
        tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
    }
    json_stream << "  \"TagSetID\": " << tag_set_id_str << ",\n";
    json_stream << "  \"Netplay\": " << std::to_string(m_game_info.netplay) << ",\n";
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, inDecode) << ",\n";
    json_stream << "  \"Away Player\": \"" << away_player_info << "\",\n"; //TODO MAKE THIS AN ID
    json_stream << "  \"Home Player\": \"" << home_player_info << "\",\n";

    json_stream << "  \"Away Score\": " << std::dec << m_game_info.away_score << ",\n";
    json_stream << "  \"Home Score\": " << std::dec << m_game_info.home_score << ",\n";

    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << ",\n";
    json_stream << "  \"Innings Played\": " << std::to_string(m_game_info.innings_played) << ",\n";
    json_stream << "  \"Quitter Team\": " << decode("QuitterTeam", m_game_info.quitter_team, inDecode) << ",\n";

    json_stream << "  \"Average Ping\": " << std::to_string(m_game_info.avg_ping) << ",\n";
    json_stream << "  \"Lag Spikes\": " << std::to_string(m_game_info.lag_spikes) << ",\n";
    json_stream << "  \"Version\": \"" << Common::GetRioRevStr() << "\",\n";

    json_stream << "  \"Character Game Stats\": {\n";

    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        u8 captain_roster_loc;
        if (team == 0){
            captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
        }
        else{ // team == 1
            captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
        }

        std::string team_string = (team == 0) ? "Away" : "Home";

        for (int roster=0; roster < cRosterSize; ++roster){
            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            
            // team integer home or away
            std::string label = "\"" + team_string + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "    " << label << "{\n";
            json_stream << "      \"Team\": \""        << std::to_string(team) << "\",\n";
            json_stream << "      \"RosterID\": "      << std::to_string(roster) << ",\n";
            json_stream << "      \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << ",\n";
            json_stream << "      \"Superstar\": "     << std::to_string(char_summary.is_starred) << ",\n";
            json_stream << "      \"Captain\": "       << std::to_string(roster == captain_roster_loc) << ",\n";
            json_stream << "      \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << ",\n";
            json_stream << "      \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << ",\n";

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "      \"Defensive Stats\": {\n";
            json_stream << "        \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << ",\n";
            json_stream << "        \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << ",\n";
            json_stream << "        \"Earned Runs\": "        << std::dec << def_stat.earned_runs << ",\n";
            json_stream << "        \"Batters Walked\": "      << def_stat.batters_walked << ",\n";
            json_stream << "        \"Batters Hit\": "         << def_stat.batters_hit << ",\n";
            json_stream << "        \"Hits Allowed\": "        << def_stat.hits_allowed << ",\n";
            json_stream << "        \"HRs Allowed\": "         << def_stat.homeruns_allowed << ",\n";
            json_stream << "        \"Pitches Thrown\": "      << def_stat.pitches_thrown << ",\n";
            json_stream << "        \"Stamina\": "             << def_stat.stamina << ",\n";
            json_stream << "        \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << ",\n";
            json_stream << "        \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << ",\n";
            json_stream << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << ",\n";
            json_stream << "        \"Big Plays\": "           << std::to_string(def_stat.big_plays) << ",\n";
            json_stream << "        \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << ",\n";
            json_stream << "        \"Batters Per Position\": [\n";

            if (m_fielder_tracker[team].battersAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].battersAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ],\n";

            json_stream << "        \"Batter Outs Per Position\": [\n";
            if (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ],\n";

            json_stream << "        \"Outs Per Position\": [\n";
            if (m_fielder_tracker[team].outsAtAnyPosition(roster, 0)){
                json_stream << "          {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "          }\n";
            }
            json_stream << "        ]\n";
            json_stream << "      },\n";

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "      \"Offensive Stats\": {\n";
            json_stream << "        \"At Bats\": "          << std::to_string(of_stat.at_bats) << ",\n";
            json_stream << "        \"Hits\": "             << std::to_string(of_stat.hits) << ",\n";
            json_stream << "        \"Singles\": "          << std::to_string(of_stat.singles) << ",\n";
            json_stream << "        \"Doubles\": "          << std::to_string(of_stat.doubles) << ",\n";
            json_stream << "        \"Triples\": "          << std::to_string(of_stat.triples) << ",\n";
            json_stream << "        \"Homeruns\": "         << std::to_string(of_stat.homeruns) << ",\n";
            json_stream << "        \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << ",\n";
            json_stream << "        \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << ",\n";
            json_stream << "        \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << ",\n";
            json_stream << "        \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << ",\n";
            json_stream << "        \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << ",\n";
            json_stream << "        \"RBI\": "              << std::to_string(of_stat.rbi) << ",\n";
            json_stream << "        \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << ",\n";
            json_stream << "        \"Star Hits\": "        << std::to_string(of_stat.star_hits) << "\n";
            json_stream << "      }\n";
            std::string commas = ((roster == 8) && (team ==1)) ? "" : ",";
            json_stream << "    }" << commas << "\n";
        }
    }
    json_stream << "  },\n";
    //=== Events === 
    json_stream << "  \"Events\": [\n";
    for (auto event_map_iter = m_game_info.events.begin(); event_map_iter != m_game_info.events.end(); event_map_iter++) {
        u8 event_num = event_map_iter->first;
        Event& event = event_map_iter->second;

        //Don't log events with inning == 0. Means game has crashed/quit and this is an empty event
        if (event.inning == 0) {
            continue;
        }

        json_stream << "    {\n";
        json_stream << "      \"Event Num\": "               << std::to_string(event_num) << ",\n";
        json_stream << "      \"Inning\": "                  << std::to_string(event.inning) << ",\n";
        json_stream << "      \"Half Inning\": "             << std::to_string(event.half_inning) << ",\n";
        json_stream << "      \"Away Score\": "              << std::dec << event.away_score << ",\n";
        json_stream << "      \"Home Score\": "              << std::dec << event.home_score << ",\n";
        json_stream << "      \"Balls\": "                   << std::to_string(event.balls) << ",\n";
        json_stream << "      \"Strikes\": "                 << std::to_string(event.strikes) << ",\n";
        json_stream << "      \"Outs\": "                    << std::to_string(event.outs) << ",\n";
        json_stream << "      \"Star Chance\": "             << std::to_string(event.is_star_chance) << ",\n";
        json_stream << "      \"Away Stars\": "              << std::to_string(event.away_stars) << ",\n";
        json_stream << "      \"Home Stars\": "              << std::to_string(event.home_stars) << ",\n";
        json_stream << "      \"Pitcher Stamina\": "         << std::to_string(event.pitcher_stamina) << ",\n";
        json_stream << "      \"Chemistry Links on Base\": " << std::to_string(event.chem_links_ob) << ",\n";
        json_stream << "      \"Pitcher Roster Loc\": "      << std::to_string(event.pitcher_roster_loc) << ",\n";
        json_stream << "      \"Batter Roster Loc\": "       << std::to_string(event.batter_roster_loc) << ",\n";
        json_stream << "      \"Catcher Roster Loc\": "       << std::to_string(event.catcher_roster_loc) << ",\n";
        json_stream << "      \"RBI\": "                     << std::to_string(event.rbi) << ",\n";
        json_stream << "      \"" << event.num_outs_during_play.name << "\": " << event.num_outs_during_play.get_key_value_string().second << ",\n";
        json_stream << "      \"Result of AB\": "            << decode("AtBatResult", event.result_of_atbat, inDecode) << ",\n";

        //=== Runners ===
        //Build vector of <Runner*, Label/Name>
        std::vector<std::pair<Runner*, std::string>> runners;
        if (event.runner_batter) {
            runners.push_back({&event.runner_batter.value(), "Batter"});
        }
        if (event.runner_1) {
            runners.push_back({&event.runner_1.value(), "1B"});
        }
        if (event.runner_2) {
            runners.push_back({&event.runner_2.value(), "2B"});
        }
        if (event.runner_3) {
            runners.push_back({&event.runner_3.value(), "3B"});
        }

        for (auto runner = runners.begin(); runner != runners.end(); runner++){
            Runner* runner_info = runner->first;
            std::string& label = runner->second;

            json_stream << "      \"Runner " << label << "\": {\n";
            json_stream << "        \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << ",\n";
            json_stream << "        \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << ",\n";
            json_stream << "        \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << ",\n";
            json_stream << "        \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << ",\n";
            json_stream << "        \"Out Location\": "        << std::to_string(runner_info->out_location) << ",\n";
            //json_stream << "        \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << ",\n";
            json_stream << "        \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << ",\n";
            json_stream << "        \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << "\n";
            std::string comma = (std::next(runner) == runners.end() && !event.pitch.has_value()) ? "" : ",";
            json_stream << "      }" << comma << "\n";
        }


        //=== Pitch ===
        if (event.pitch.has_value()){
            Pitch* pitch = &event.pitch.value();
            json_stream << "      \"Pitch\": {\n";
            json_stream << "        \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << ",\n";
            json_stream << "        \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << ",\n";
            json_stream << "        \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << ",\n";
            json_stream << "        \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << ",\n";
            json_stream << "        \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << ",\n";
            json_stream << "        \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << ",\n";
            json_stream << "        \"Ball Position - Strikezone\": "   << floatConverter(pitch->ball_z_strike_vs_ball) << ",\n";
            json_stream << "        \"In Strikezone\": "      << std::to_string(pitch->ball_in_strikezone) << ",\n";
            json_stream << "        \"" << pitch->bat_contact_x_pos.name << "\": " << floatConverter(pitch->bat_contact_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << pitch->bat_contact_z_pos.name << "\": " << floatConverter(pitch->bat_contact_z_pos.get_value()) << ",\n";
            json_stream << "        \"DB\": "                 << std::to_string(pitch->db) << ",\n";
            json_stream << "        \"Type of Swing\": "      << decode("Swing", pitch->type_of_swing, inDecode);
            
            //=== Contact ===
            if (pitch->contact.has_value() && pitch->contact->type_of_contact.get_value() != 0xFF){
                json_stream << ",\n";

                Contact* contact = &pitch->contact.value();
                json_stream << "        \"Contact\": {\n";
                json_stream << "          \"" << contact->type_of_contact.name << "\":" << decode("Contact", contact->type_of_contact.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->charge_power_up.name << "\": " << floatConverter(contact->charge_power_up.get_value()) << ",\n";
                json_stream << "          \"" << contact->charge_power_down.name << "\": " << floatConverter(contact->charge_power_down.get_value()) << ",\n";
                json_stream << "          \"" << contact->moon_shot.name << "\": " << contact->moon_shot.get_key_value_string().second << ",\n"; 
                json_stream << "          \"" << contact->input_direction_push_pull.name << "\": " << decode("Stick", contact->input_direction_push_pull.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->input_direction_stick.name << "\": " << decode("StickVec", contact->input_direction_stick.get_value(), inDecode) << ",\n";
                json_stream << "          \"" << contact->frame_of_swing.name << "\": \"" << std::dec << contact->frame_of_swing.get_value() << "\",\n";

                json_stream << "          \"" << contact->power.name << "\": \"" << std::dec << contact->power.get_value() <<"\",\n";
                json_stream << "          \"" << contact->vert_angle.name << "\": \"" << std::dec << contact->vert_angle.get_value() << "\",\n";
                json_stream << "          \"" << contact->horiz_angle.name << "\": \"" << std::dec << contact->horiz_angle.get_value() << "\",\n";

                json_stream << "          \"" << contact->contact_absolute.name << "\": " << floatConverter(contact->contact_absolute.get_value()) << ",\n";
                json_stream << "          \"" << contact->contact_quality.name << "\": " << floatConverter(contact->contact_quality.get_value()) << ",\n";
                
                json_stream << "          \"" << contact->rng1.name << "\": \"" << std::dec << contact->rng1.get_value() << "\",\n";
                json_stream << "          \"" << contact->rng2.name << "\": \"" << std::dec << contact->rng2.get_value() << "\",\n";
                json_stream << "          \"" << contact->rng3.name << "\": \"" << std::dec << contact->rng3.get_value() << "\",\n";

                json_stream << "          \"" << contact->ball_x_velo.name << "\": " << floatConverter(contact->ball_x_velo.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_y_velo.name << "\": " << floatConverter(contact->ball_y_velo.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_z_velo.name << "\": " << floatConverter(contact->ball_z_velo.get_value()) << ",\n";

                json_stream << "          \"" << contact->ball_contact_x_pos.name << "\": " << floatConverter(contact->ball_contact_x_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_contact_z_pos.name << "\": " << floatConverter(contact->ball_contact_z_pos.get_value()) << ",\n";
                                
                json_stream << "          \"" << contact->ball_x_pos.name << "\": " << floatConverter(contact->ball_x_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_y_pos.name << "\": " << floatConverter(contact->ball_y_pos.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_z_pos.name << "\": " << floatConverter(contact->ball_z_pos.get_value()) << ",\n";

                json_stream << "          \"" << contact->ball_max_height.name << "\": " << floatConverter(contact->ball_max_height.get_value()) << ",\n";
                json_stream << "          \"" << contact->ball_hang_time.name << "\": \"" << std::dec << contact->ball_hang_time.get_value() << "\",\n";
                json_stream << "          \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << ",\n";
                json_stream << "          \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

                //=== Fielder ===
                //TODO could be reworked
                if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                    json_stream << ",\n";

                    //First fielder to touch the ball
                    Fielder* fielder;

                    //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                    
                    if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                    else {fielder = &contact->collect_fielder.value();}

                    json_stream << "          \"First Fielder\": {\n";
                    json_stream << "            \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << ",\n";
                    json_stream << "            \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << ",\n";
                    json_stream << "            \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << ",\n";
                    json_stream << "            \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << ",\n";
                    json_stream << "            \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << ",\n";
                    json_stream << "            \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << ",\n";
                    json_stream << "            \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_arg, inDecode) << ",\n";
                    json_stream << "            \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << ",\n";
                    json_stream << "            \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << ",\n";
                    json_stream << "            \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << ",\n";
                    json_stream << "            \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << "\n";
                    json_stream << "          }\n";
                    /*
                    else if (contact->first_fielder.has_value() 
                            && (contact->first_fielder->fielder_roster_loc != contact->collect_fielder->fielder_roster_loc)) {
                        
                        Fielder* first_fielder  = &contact->first_fielder.value();
                        Fielder* second_fielder = &contact->collect_fielder.value();

                        json_stream << "          \"First Fielder\": {\n";
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(first_fielder->fielder_roster_loc) << ",\n";
                        json_stream << "            \"Fielder Position\": "        << std::to_string(first_fielder->fielder_pos) << ",\n";
                        json_stream << "            \"Fielder Character\": "       << std::to_string(first_fielder->fielder_char_id) << ",\n";
                        json_stream << "            \"Fielder Action\": "          << std::to_string(first_fielder->fielder_action) << ",\n";
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(first_fielder->fielder_swapped_for_batter) << ",\n";
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(first_fielder->fielder_x_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(first_fielder->fielder_y_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(first_fielder->fielder_z_pos) << ",\n";
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(first_fielder->bobble) << "\n";
                        json_stream << "          },\n";
                        json_stream << "          \"Second Fielder\": {\n";
                        json_stream << "            \"Fielder Roster Location\": " << std::to_string(second_fielder->fielder_roster_loc) << ",\n";
                        json_stream << "            \"Fielder Position\": "        << std::to_string(second_fielder->fielder_pos) << ",\n";
                        json_stream << "            \"Fielder Character\": "       << std::to_string(second_fielder->fielder_char_id) << ",\n";
                        json_stream << "            \"Fielder Action\": "          << std::to_string(second_fielder->fielder_action) << ",\n";
                        json_stream << "            \"Fielder Swap\": "            << std::to_string(second_fielder->fielder_swapped_for_batter) << ",\n";
                        json_stream << "            \"Fielder Position - X\": "    << floatConverter(second_fielder->fielder_x_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Y\": "    << floatConverter(second_fielder->fielder_y_pos) << ",\n";
                        json_stream << "            \"Fielder Position - Z\": "    << floatConverter(second_fielder->fielder_z_pos) << ",\n";
                        json_stream << "            \"Fielder Bobble\": "          << std::to_string(second_fielder->bobble) << "\n";
                        json_stream << "          }\n";
                    }
                    */
                }
                else{ //Finish contact section
                    json_stream << "\n";
                }
                json_stream << "        }\n";
            }
            else { //Finish pitch section
                json_stream << "\n";
            }
            json_stream << "      }\n";
        }

        std::string end_comma = (std::next(event_map_iter) !=  m_game_info.events.end()) ? "," : "";
        json_stream << "    }" << end_comma << "\n";
    }

    json_stream << "  ]\n";
    json_stream << "}\n";

    return json_stream.str();
}

std::string StatTracker::getHUDJSON(std::string in_event_num, Event& in_curr_event, std::optional<Event> in_prev_event, bool inDecode){
    std::stringstream json_stream;

    if (in_curr_event.inning == 0) {
        return "{}";
    }

    json_stream << "{\n";

    json_stream << "  \"Event Num\": \""             << in_event_num << "\",\n";
    json_stream << "  \"Away Player\": \""           << m_game_info.getAwayTeamPlayer().GetUsername() << "\",\n";
    json_stream << "  \"Home Player\": \""           << m_game_info.getHomeTeamPlayer().GetUsername() << "\",\n";
    json_stream << "  \"Inning\": "                  << std::to_string(in_curr_event.inning) << ",\n";
    json_stream << "  \"Half Inning\": "             << std::to_string(in_curr_event.half_inning) << ",\n";
    json_stream << "  \"Away Score\": "              << std::dec << in_curr_event.away_score << ",\n";
    json_stream << "  \"Home Score\": "              << std::dec << in_curr_event.home_score << ",\n";
    json_stream << "  \"Balls\": "                   << std::to_string(in_curr_event.balls) << ",\n";
    json_stream << "  \"Strikes\": "                 << std::to_string(in_curr_event.strikes) << ",\n";
    json_stream << "  \"Outs\": "                    << std::to_string(in_curr_event.outs) << ",\n";
    json_stream << "  \"Star Chance\": "             << std::to_string(in_curr_event.is_star_chance) << ",\n";
    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << ",\n";
    json_stream << "  \"Pitcher Stamina\": "         << std::to_string(in_curr_event.pitcher_stamina) << ",\n";
    json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << ",\n";
    json_stream << "  \"" << in_curr_event.num_outs_during_play.name << "\": " << in_curr_event.num_outs_during_play.get_key_value_string().second << ",\n";
    json_stream << "  \"Pitcher Roster Loc\": "      << std::to_string(in_curr_event.pitcher_roster_loc) << ",\n";
    json_stream << "  \"Batter Roster Loc\": "       << std::to_string(in_curr_event.batter_roster_loc) << ",\n";

    for (int team=0; team < 2; ++team){
        for (int roster=0; roster < cRosterSize; ++roster){

            u8 captain_roster_loc = 0;
            if (team == 0){
                captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            }
            else{ // team == 1
                captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
            }

            std::string team_string = (team == 0) ? "Away" : "Home";

            CharacterSummary& char_summary = m_game_info.character_summaries[team][roster];
            std::string label = "\"" + team_string + " Roster " + std::to_string(roster) + "\": ";
            json_stream << "  " << label << "{\n";
            json_stream << "    \"Team\": \""        << std::to_string(team) << "\",\n";
            json_stream << "    \"RosterID\": "      << std::to_string(roster) << ",\n";
            json_stream << "    \"CharID\": "        << decode("Character", char_summary.char_id, inDecode) << ",\n";
            json_stream << "    \"Superstar\": "     << std::to_string(char_summary.is_starred) << ",\n";
            json_stream << "    \"Captain\": "       << std::to_string(roster == captain_roster_loc) << ",\n";
            json_stream << "    \"Fielding Hand\": " << decode("Hand", char_summary.fielding_hand, inDecode) << ",\n";
            json_stream << "    \"Batting Hand\": "  << decode("Hand", char_summary.batting_hand, inDecode) << ",\n";

            //=== Defensive Stats ===
            EndGameRosterDefensiveStats& def_stat = char_summary.end_game_defensive_stats;
            json_stream << "    \"Defensive Stats\": {\n";
            json_stream << "      \"Batters Faced\": "       << std::to_string(def_stat.batters_faced) << ",\n";
            json_stream << "      \"Runs Allowed\": "        << std::dec << def_stat.runs_allowed << ",\n";
            json_stream << "      \"Earned Runs\": "        << std::dec << def_stat.earned_runs << ",\n";
            json_stream << "      \"Batters Walked\": "      << def_stat.batters_walked << ",\n";
            json_stream << "      \"Batters Hit\": "         << def_stat.batters_hit << ",\n";
            json_stream << "      \"Hits Allowed\": "        << def_stat.hits_allowed << ",\n";
            json_stream << "      \"HRs Allowed\": "         << def_stat.homeruns_allowed << ",\n";
            json_stream << "      \"Pitches Thrown\": "      << def_stat.pitches_thrown << ",\n";
            json_stream << "      \"Stamina\": "             << def_stat.stamina << ",\n";
            json_stream << "      \"Was Pitcher\": "         << std::to_string(def_stat.was_pitcher) << ",\n";
            json_stream << "      \"Strikeouts\": "          << std::to_string(def_stat.strike_outs) << ",\n";
            json_stream << "      \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << ",\n";
            json_stream << "      \"Big Plays\": "           << std::to_string(def_stat.big_plays) << ",\n";
            json_stream << "      \"Outs Pitched\": "        << std::to_string(def_stat.outs_pitched) << ",\n";
            json_stream << "      \"Batters Per Position\": [\n";

            if (m_fielder_tracker[team].battersAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].battersAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ],\n";

            json_stream << "      \"Batter Outs Per Position\": [\n";
            if (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].batterOutsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].batter_outs_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ],\n";

            json_stream << "      \"Outs Per Position\": [\n";
            if (m_fielder_tracker[team].outsAtAnyPosition(roster, 0)){
                json_stream << "        {\n";
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos] > 0){
                        std::string comma = (m_fielder_tracker[team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].fielder_map[roster].out_count_by_position[pos]) << comma << "\n";
                    }
                }
                json_stream << "        }\n";
            }
            json_stream << "      ]\n";
            json_stream << "    },\n";

            //=== Offensive Stats ===
            EndGameRosterOffensiveStats& of_stat = char_summary.end_game_offensive_stats;
            json_stream << "    \"Offensive Stats\": {\n";
            json_stream << "      \"At Bats\": "          << std::to_string(of_stat.at_bats) << ",\n";
            json_stream << "      \"Hits\": "             << std::to_string(of_stat.hits) << ",\n";
            json_stream << "      \"Singles\": "          << std::to_string(of_stat.singles) << ",\n";
            json_stream << "      \"Doubles\": "          << std::to_string(of_stat.doubles) << ",\n";
            json_stream << "      \"Triples\": "          << std::to_string(of_stat.triples) << ",\n";
            json_stream << "      \"Homeruns\": "         << std::to_string(of_stat.homeruns) << ",\n";
            json_stream << "      \"Successful Bunts\": " << std::to_string(of_stat.successful_bunts) << ",\n";
            json_stream << "      \"Sac Flys\": "         << std::to_string(of_stat.sac_flys) << ",\n";
            json_stream << "      \"Strikeouts\": "       << std::to_string(of_stat.strikouts) << ",\n";
            json_stream << "      \"Walks (4 Balls)\": "  << std::to_string(of_stat.walks_4balls) << ",\n";
            json_stream << "      \"Walks (Hit)\": "      << std::to_string(of_stat.walks_hit) << ",\n";
            json_stream << "      \"RBI\": "              << std::to_string(of_stat.rbi) << ",\n";
            json_stream << "      \"Bases Stolen\": "     << std::to_string(of_stat.bases_stolen) << ",\n";
            json_stream << "      \"Star Hits\": "        << std::to_string(of_stat.star_hits) << "\n";
            json_stream << "    }\n";
            json_stream << "  },\n";
        }
    }

    //=== Runners ===
    //Build vector of <Runner*, Label/Name>
    std::vector<std::pair<Runner*, std::string>> runners;
    if (in_curr_event.runner_batter) {
        runners.push_back({&in_curr_event.runner_batter.value(), "Batter"});
    }
    if (in_curr_event.runner_1) {
        runners.push_back({&in_curr_event.runner_1.value(), "1B"});
    }
    if (in_curr_event.runner_2) {
        runners.push_back({&in_curr_event.runner_2.value(), "2B"});
    }
    if (in_curr_event.runner_3) {
        runners.push_back({&in_curr_event.runner_3.value(), "3B"});
    }

    for (auto runner = runners.begin(); runner != runners.end(); runner++){
        Runner* runner_info = runner->first;
        std::string& label = runner->second;

        json_stream << "  \"Runner " << label << "\": {\n";
        json_stream << "    \"Runner Roster Loc\": "   << std::to_string(runner_info->roster_loc) << ",\n";
        json_stream << "    \"Runner Char Id\": "      << decode("Character", runner_info->char_id, inDecode) << ",\n";
        json_stream << "    \"Runner Initial Base\": " << std::to_string(runner_info->initial_base) << ",\n";
        json_stream << "    \"Out Type\": "            << decode("Out", runner_info->out_type, inDecode) << ",\n";
        json_stream << "    \"Out Location\": "        << std::to_string(runner_info->out_location) << ",\n";
        //json_stream << "    \"Runner Basepath Location\": "  << std::to_string(runner_info->basepath_location) << ",\n";
        json_stream << "    \"Steal\": "               << decode("Steal", runner_info->steal, inDecode) << ",\n";
        json_stream << "    \"Runner Result Base\": "  << std::to_string(runner_info->result_base) << "\n";
        std::string comma = (std::next(runner) == runners.end() && !in_prev_event.has_value() ) ? "" : ",";
        json_stream << "  }" << comma << "\n";
    }

    //Previous Event - return if first event of game. Else write the event
    if (!in_prev_event.has_value()){
        json_stream << "}";
        return json_stream.str();
    }

    //=== Pitch ===

    json_stream << "  \"Previous Event\": {\n";
    json_stream << "    \"RBI\": "                     << std::to_string(in_prev_event->rbi) << ",\n";
    std::string comma = (in_prev_event->pitch.has_value()) ? "," : "";
    json_stream << "    \"Result of AB\": "            << decode("AtBatResult", in_prev_event->result_of_atbat, inDecode) << comma << "\n";
    if (in_prev_event->pitch.has_value()){
        Pitch* pitch = &in_prev_event->pitch.value();
        json_stream << "    \"Pitch\": {\n";
        json_stream << "      \"Pitcher Team Id\": "    << std::to_string(pitch->pitcher_team_id) << ",\n";
        json_stream << "      \"Pitcher Char Id\": "    << decode("Character", pitch->pitcher_char_id, inDecode) << ",\n";
        json_stream << "      \"Pitch Type\": "         << decode("Pitch", pitch->pitch_type, inDecode) << ",\n";
        json_stream << "      \"Charge Type\": "        << decode("ChargePitch", pitch->charge_type, inDecode) << ",\n";
        json_stream << "      \"Star Pitch\": "         << std::to_string(pitch->star_pitch) << ",\n";
        json_stream << "      \"Pitch Speed\": "        << std::to_string(pitch->pitch_speed) << ",\n";
        json_stream << "      \"Ball Position - Strikezone\": "   << floatConverter(pitch->ball_z_strike_vs_ball) << ",\n";
        json_stream << "      \"In Strikezone\": "      << std::to_string(pitch->ball_in_strikezone) << ",\n";
        json_stream << "        \"" << pitch->bat_contact_x_pos.name << "\": " << floatConverter(pitch->bat_contact_x_pos.get_value()) << ",\n";
        json_stream << "        \"" << pitch->bat_contact_z_pos.name << "\": " << floatConverter(pitch->bat_contact_z_pos.get_value()) << ",\n";
        json_stream << "      \"DB\": "                 << std::to_string(pitch->db) << ",\n";
        json_stream << "      \"Type of Swing\": "      << decode("Swing", pitch->type_of_swing, inDecode);
        
        //=== Contact ===
        if (pitch->contact.has_value() && pitch->contact->type_of_contact.get_value() != 0xFF){
            json_stream << ",\n";

            Contact* contact = &pitch->contact.value();
            json_stream << "      \"Contact\": {\n";
            json_stream << "        \"" << contact->type_of_contact.name << "\":" << decode("Contact", contact->type_of_contact.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->charge_power_up.name << "\": " << floatConverter(contact->charge_power_up.get_value()) << ",\n";
            json_stream << "        \"" << contact->charge_power_down.name << "\": " << floatConverter(contact->charge_power_down.get_value()) << ",\n";
            json_stream << "        \"" << contact->moon_shot.name << "\": " << contact->moon_shot.get_key_value_string().second << ",\n"; 
            json_stream << "        \"" << contact->input_direction_push_pull.name << "\": " << decode("Stick", contact->input_direction_push_pull.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->input_direction_stick.name << "\": " << decode("StickVec", contact->input_direction_stick.get_value(), inDecode) << ",\n";
            json_stream << "        \"" << contact->frame_of_swing.name << "\": \"" << std::dec << contact->frame_of_swing.get_value() << "\",\n";
            json_stream << "        \"" << contact->power.name << "\": \"" << std::dec << contact->power.get_value() <<"\",\n";
            json_stream << "        \"" << contact->vert_angle.name << "\": \"" << std::dec << contact->vert_angle.get_value() << "\",\n";
            json_stream << "        \"" << contact->horiz_angle.name << "\": \"" << std::dec << contact->horiz_angle.get_value() << "\",\n";
            json_stream << "        \"" << contact->contact_absolute.name << "\": " << floatConverter(contact->contact_absolute.get_value()) << ",\n";
            json_stream << "        \"" << contact->contact_quality.name << "\": " << floatConverter(contact->contact_quality.get_value()) << ",\n";
            json_stream << "        \"" << contact->rng1.name << "\": \"" << std::dec << contact->rng1.get_value() << "\",\n";
            json_stream << "        \"" << contact->rng2.name << "\": \"" << std::dec << contact->rng2.get_value() << "\",\n";
            json_stream << "        \"" << contact->rng3.name << "\": \"" << std::dec << contact->rng3.get_value() << "\",\n";
            json_stream << "        \"" << contact->ball_x_velo.name << "\": " << floatConverter(contact->ball_x_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_y_velo.name << "\": " << floatConverter(contact->ball_y_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_z_velo.name << "\": " << floatConverter(contact->ball_z_velo.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_contact_x_pos.name << "\": " << floatConverter(contact->ball_contact_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_contact_z_pos.name << "\": " << floatConverter(contact->ball_contact_z_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_x_pos.name << "\": " << floatConverter(contact->ball_x_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_y_pos.name << "\": " << floatConverter(contact->ball_y_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_z_pos.name << "\": " << floatConverter(contact->ball_z_pos.get_value()) << ",\n";
            json_stream << "        \"" << contact->ball_hang_time.name << "\": " << std::dec << contact->ball_hang_time.get_value() << ",\n";
            json_stream << "        \"" << contact->ball_max_height.name << "\": " << floatConverter(contact->ball_max_height.get_value()) << ",\n";
            json_stream << "        \"Contact Result - Primary\": "         << decode("PrimaryContactResult", contact->primary_contact_result, inDecode) << ",\n";
            json_stream << "        \"Contact Result - Secondary\": "       << decode("SecondaryContactResult", contact->secondary_contact_result, inDecode);

            //=== Fielder ===
            //TODO could be reworked
            if (contact->first_fielder.has_value() || contact->collect_fielder.has_value()){
                json_stream << ",\n";

                //First fielder to touch the ball
                Fielder* fielder;

                //If the fielder bobbled but the same fielder collected the ball OR there was no bobble, log single fielder
                
                if (contact->first_fielder.has_value()) { fielder = &contact->first_fielder.value(); }
                else {fielder = &contact->collect_fielder.value();}

                json_stream << "        \"First Fielder\": {\n";
                json_stream << "          \"Fielder Roster Location\": " << std::to_string(fielder->fielder_roster_loc) << ",\n";
                json_stream << "          \"Fielder Position\": "        << decode("Position", fielder->fielder_pos, inDecode) << ",\n";
                json_stream << "          \"Fielder Character\": "       << decode("Character", fielder->fielder_char_id, inDecode) << ",\n";
                json_stream << "          \"Fielder Action\": "          << decode("Action", fielder->fielder_action, inDecode) << ",\n";
                json_stream << "          \"Fielder Jump\": "            << std::to_string(fielder->fielder_jump) << ",\n";
                json_stream << "          \"Fielder Swap\": "            << std::to_string(fielder->fielder_swapped_for_batter) << ",\n";
                json_stream << "          \"Fielder Manual Selected\": " << decode("ManualSelect", fielder->fielder_manual_select_arg, inDecode) << ",\n";
                json_stream << "          \"Fielder Position - X\": "    << floatConverter(fielder->fielder_x_pos) << ",\n";
                json_stream << "          \"Fielder Position - Y\": "    << floatConverter(fielder->fielder_y_pos) << ",\n";
                json_stream << "          \"Fielder Position - Z\": "    << floatConverter(fielder->fielder_z_pos) << ",\n";
                json_stream << "          \"Fielder Bobble\": "          << decode("Bobble", fielder->bobble, inDecode) << "\n";
                json_stream << "        }\n";
            }
            else{ //Finish contact section
                json_stream << "\n";
            }
            json_stream << "      }\n"; //close contact
        }
        else { //Finish pitch section
            json_stream << "\n";
        }
        json_stream << "    }\n"; //Close pitch
    }
    json_stream << "  }\n"; //Close Previous Event
    json_stream << "}";
    return json_stream.str();
}

//Scans player for possession
std::optional<StatTracker::Fielder> StatTracker::logFielderWithBall() {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderControlStatus = aFielder_ControlStatus + (pos * cFielder_Offset);
        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);

        bool fielder_has_ball = (PowerPC::HostRead_U8(aFielderControlStatus) == 0xA);

        if (fielder_has_ball) {
            Fielder fielder_with_ball;
            //get char id
            fielder_with_ball.fielder_roster_loc = PowerPC::HostRead_U8(aFielderRosterLoc);
            fielder_with_ball.fielder_char_id = PowerPC::HostRead_U8(aFielderCharId);
            fielder_with_ball.fielder_pos = pos;

            fielder_with_ball.fielder_x_pos = PowerPC::HostRead_U32(aFielderPosX);
            fielder_with_ball.fielder_y_pos = PowerPC::HostRead_U32(aFielderPosY);
            fielder_with_ball.fielder_z_pos = PowerPC::HostRead_U32(aFielderPosZ);

            if (PowerPC::HostRead_U8(aFielderAction)) {
                fielder_with_ball.fielder_action = PowerPC::HostRead_U8(aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (PowerPC::HostRead_U8(aFielderJump)) {
                fielder_with_ball.fielder_jump = PowerPC::HostRead_U8(aFielderJump); //1 = jump
            }

            fielder_with_ball.fielder_manual_select_arg = PowerPC::HostRead_U8(aFielder_ManualSelectArg);

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_with_ball.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_with_ball.fielder_action)
                      << " Manual Select=" << std::to_string(fielder_with_ball.fielder_manual_select_arg)
                      << " Jump=" << std::to_string(fielder_with_ball.fielder_jump) << "\n";

            std::cout << "Logging Fielder\n";
            fielder = std::make_optional(fielder_with_ball);
            return fielder;
        }
    }
    return std::nullopt;
}

std::optional<StatTracker::Fielder> StatTracker::logFielderBobble() {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderBobbleStatus = aFielder_Bobble + (pos * cFielder_Offset);
        u32 aFielderKnockoutStatus = aFielder_Knockout + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderRosterLoc = aFielder_RosterLoc + (pos * cFielder_Offset);
        u32 aFielderCharId = aFielder_CharId + (pos * cFielder_Offset);
        
        u8 typeOfFielderDisruption = 0x0;
        u8 bobble_addr = PowerPC::HostRead_U8(aFielderBobbleStatus);
        u8 knockout_addr = PowerPC::HostRead_U8(aFielderKnockoutStatus);

        if (knockout_addr) {
            typeOfFielderDisruption = 0x10; //Knockout - no bobble
        }
        else if (bobble_addr){
            typeOfFielderDisruption = bobble_addr; //Different types of bobbles
        }

        if (typeOfFielderDisruption > 0x1) {
            Fielder fielder_that_bobbled;
            //get char id
            fielder_that_bobbled.fielder_roster_loc = PowerPC::HostRead_U8(aFielderRosterLoc);
            fielder_that_bobbled.fielder_char_id = PowerPC::HostRead_U8(aFielderCharId);

            fielder_that_bobbled.fielder_x_pos = PowerPC::HostRead_U32(aFielderPosX);
            fielder_that_bobbled.fielder_y_pos = PowerPC::HostRead_U32(aFielderPosY);
            fielder_that_bobbled.fielder_z_pos = PowerPC::HostRead_U32(aFielderPosZ);
            fielder_that_bobbled.fielder_pos = pos;
            fielder_that_bobbled.bobble = typeOfFielderDisruption;

            if (PowerPC::HostRead_U8(aFielderAction)) {
                fielder_that_bobbled.fielder_action = PowerPC::HostRead_U8(aFielderAction); //2 = Slide, 3 = Walljump
            }
            if (PowerPC::HostRead_U8(aFielderJump)) {
                fielder_that_bobbled.fielder_jump = PowerPC::HostRead_U8(aFielderJump); //1 = jump
            }

            //We can read manual select now because we don't have the ball
            fielder_that_bobbled.fielder_manual_select_arg = PowerPC::HostRead_U8(aFielder_ManualSelectArg);

            std::cout << "Fielder Pos=" << std::to_string(pos) << " Fielder RosterLoc=" << std::to_string(fielder_that_bobbled.fielder_roster_loc)
                      << " Fielder Action: " << std::to_string(fielder_that_bobbled.fielder_action) 
                      << " Jump=" << std::to_string(fielder_that_bobbled.fielder_jump)
                      << " Manual Select=" << std::to_string(fielder_that_bobbled.fielder_manual_select_arg)
                      << " Bobble=" << std::to_string(fielder_that_bobbled.bobble) << "\n";
                      
            fielder = std::make_optional(fielder_that_bobbled);
            return fielder;
        }
    }
    return std::nullopt;
}

//Read players from ini file and assign to team
void StatTracker::readPlayerNames(bool local_game) {
  int team0_port = m_game_info.team0_port;
  int team1_port = m_game_info.team1_port;

  if (local_game)
  {
    // Player 1
    if (team0_port == 0)
        m_game_info.team0_player = LocalPlayers::m_local_player_1;
    else {
        m_game_info.team0_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team0_player.username = "CPU";
        m_game_info.team0_player.userid = "CPU";
    }

    // other player
    if (team1_port == 1)
        m_game_info.team1_player = LocalPlayers::m_local_player_2;
    else if (team1_port == 2)
        m_game_info.team1_player = LocalPlayers::m_local_player_3;
    else if (team1_port == 3)
        m_game_info.team1_player = LocalPlayers::m_local_player_4;
    else { // 4/5 are CPUs
        m_game_info.team1_player = LocalPlayers::LocalPlayers::Player();
        m_game_info.team1_player.username = "CPU";
        m_game_info.team1_player.userid = "CPU";
    }
  }

  else
  {
    m_game_info.team0_player = m_game_info.NetplayerUserInfo[team0_port + 1];
    m_game_info.team1_player = m_game_info.NetplayerUserInfo[team1_port + 1];
  }
}


void StatTracker::setTagSetId(Tag::TagSet tag_set, bool netplay) {
    std::cout << "TagSet Id=" << tag_set.id << "," << "TagSet Name=" << tag_set.name << "\n";
    netplay ? m_state.tag_set_id_netplay = tag_set.id : m_state.tag_set_id_local = tag_set.id;
}

void StatTracker::clearTagSetId(bool netplay) {
    std::cout << "Clearing TagSet" << "\n";
    netplay ? m_state.tag_set_id_netplay = std::nullopt : m_state.tag_set_id_local = std::nullopt;
}


bool StatTracker::shouldSubmitGame() {
    bool cpuInGame = (m_game_info.getAwayTeamPlayer().GetUserID() == "CPU") || (m_game_info.getHomeTeamPlayer().GetUserID() == "CPU");
    bool tag_set_game = m_game_info.tag_set_id.has_value();
    std::cout << "Checking game submission. TagSetSelected=" << tag_set_game << " cpuInGame=" << cpuInGame << "\n";

    return (!cpuInGame && tag_set_game);
}

void StatTracker::setNetplaySession(bool netplay_session, bool is_host, std::string opponent_name){
    m_state.m_netplay_session = netplay_session;
    m_state.m_is_host = is_host;
    m_state.m_netplay_opponent_alias = opponent_name;
}

void StatTracker::setAvgPing(int avgPing)
{
  //std::cout << "Avg Ping=" << avgPing << "\n";
  m_game_info.avg_ping = avgPing;
}

void StatTracker::setLagSpikes(int nLagSpikes)
{
  //std::cout << "Number of Lag Spikes=" << nLagSpikes << "\n";
  m_game_info.lag_spikes = nLagSpikes;
}
void StatTracker::setNetplayerUserInfo(std::map<int, LocalPlayers::LocalPlayers::Player> userInfo)
{
  for (auto player : userInfo)
    m_game_info.NetplayerUserInfo[player.first] = player.second;
}

void StatTracker::setGameID(u32 gameID)
{
  m_game_info.game_id = gameID;
}

void StatTracker::initPlayerInfo(){
    //Read start time
    std::time_t unix_time = std::time(nullptr);
    m_game_info.start_unix_date_time = std::to_string(unix_time);
    m_game_info.start_local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.start_local_date_time.pop_back();
    //Collect port info for players
    if (m_game_info.team0_port == 0xFF && m_game_info.team1_port == 0xFF){
        //From Roeming
        std::array<u8, 2> ports = {PowerPC::HostRead_U8(0x800e874c), PowerPC::HostRead_U8(0x800e874d)};
        
        u8 BattingPort = ports[PowerPC::HostRead_U32(0x80892990)];
        u8 FieldingPort = ports[PowerPC::HostRead_U32(0x80892994)];
        
        m_game_info.team0_port = ports[0];
        m_game_info.team1_port = ports[1];

        //This will be true for event 0 (when this function is executed)
        m_game_info.home_port = FieldingPort;
        m_game_info.away_port = BattingPort;

        readPlayerNames(!m_game_info.netplay);

        std::string away_player_name;
        std::string home_player_name;
        if (m_game_info.away_port == m_game_info.team0_port) {
            away_player_name = m_game_info.team0_player.GetUsername();
            home_player_name = m_game_info.team1_player.GetUsername();
        }
        else{
            away_player_name = m_game_info.team1_player.GetUsername();
            home_player_name = m_game_info.team0_player.GetUsername();
        }

        std::cout << "ports[0]=" << std::to_string(PowerPC::HostRead_U8(0x800e874c)) << " ports[1]=" << std::to_string(PowerPC::HostRead_U8(0x800e874d)) << "\n";
        std::cout << "BattingPort=" << std::to_string(PowerPC::HostRead_U32(0x80892990)) << " FieldingPort=" << std::to_string(PowerPC::HostRead_U32(0x80892994)) << "\n";

        std::cout << "Info:  Fielder Port=" << std::to_string(FieldingPort) << ", Batter Port=" << std::to_string(BattingPort) << "\n";
        std::cout << "Info:  Team0 Port=" << std::to_string(m_game_info.team0_port) << ", Team1 Port=" << std::to_string(m_game_info.team1_port) << "\n";
        std::cout << "Info:  Away Port=" << std::to_string(m_game_info.away_port) << ", Home Port=" << std::to_string(m_game_info.home_port) << "\n";
        std::cout << "Info:  Away Player=" << (away_player_name) << ", Home Player=" << (home_player_name) << "\n";

        initCaptains();
    }
}

void StatTracker::initCaptains(){
    m_game_info.team0_captain_roster_loc = PowerPC::HostRead_U8(aTeam0_Captain_Roster_Loc);
    m_game_info.team1_captain_roster_loc = PowerPC::HostRead_U8(aTeam1_Captain_Roster_Loc);

    u8 away_captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
    u8 home_captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;

    std::cout << "Info:  Team0 Captain=" << std::to_string(m_game_info.team0_captain_roster_loc) << ", Home Captain=" << (std::to_string(m_game_info.team1_captain_roster_loc)) << "\n";
    std::cout << "Info:  Away Captain=" << std::to_string(away_captain_roster_loc) << ", Home Captain=" << (std::to_string(home_captain_roster_loc)) << "\n\n";
}

void StatTracker::onGameQuit(){
    u8 quitter_port = PowerPC::HostRead_U8(aWhoQuit);
    m_game_info.quitter_team = (quitter_port == m_game_info.away_port);
    logGameInfo();

    std::cout << "Quit detected\n";

    //Game has ended. Write file but do not submit
    std::string jsonPath = getStatJsonPath("quit.decode.");
    std::string json = getStatJSON(true);
    
    File::WriteStringToFile(jsonPath, json);

    jsonPath = getStatJsonPath("quit.");
    json = getStatJSON(false, true);
    
    File::WriteStringToFile(jsonPath, json);

    // json = getStatJSON(false, false);
    // if (shouldSubmitGame()) {
    //     const Common::HttpRequest::Response response =
    //     m_http.Post("https://api.projectrio.app/populate_db/", json,
    //         {
    //             {"Content-Type", "application/json"},
    //         }
    //     );
    // }
}

std::optional<StatTracker::Runner> StatTracker::logRunnerInfo(u8 base){
    std::optional<Runner> runner;
    //See if there is a runner in this pos
    if (PowerPC::HostRead_U8(aRunner_RosterLoc + (base * cRunner_Offset)) != 0xFF){
        Runner init_runner;
        init_runner.roster_loc = PowerPC::HostRead_U8(aRunner_RosterLoc + (base * cRunner_Offset));
        init_runner.char_id = PowerPC::HostRead_U8(aRunner_CharId + (base * cRunner_Offset));
        init_runner.initial_base = base;
        init_runner.basepath_location = PowerPC::HostRead_U32(aRunner_BasepathPercentage + (base * cRunner_Offset));
        runner = std::make_optional(init_runner);
        return runner;        
    }
    return runner;
}

bool StatTracker::anyRunnerStealing(Event& in_event){
    u8 runner_1_stealing = PowerPC::HostRead_U8(aRunner_Stealing + (1 * cRunner_Offset));
    u8 runner_2_stealing = PowerPC::HostRead_U8(aRunner_Stealing + (2 * cRunner_Offset));
    u8 runner_3_stealing = PowerPC::HostRead_U8(aRunner_Stealing + (3 * cRunner_Offset));

    return (runner_1_stealing || runner_2_stealing || runner_3_stealing);
}

void StatTracker::logRunnerEvents(Runner* in_runner){
    //Return if no runner
    if (in_runner->out_type != 0 ) { return; }

    //Return if runner has already gotten out
    in_runner->out_type = PowerPC::HostRead_U8(aRunner_OutType + (in_runner->initial_base * cRunner_Offset));
    if (in_runner->out_type != 0) {
        in_runner->out_location = PowerPC::HostRead_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
        in_runner->result_base = 0xFF;
        in_runner->basepath_location = PowerPC::HostRead_U32(aRunner_BasepathPercentage + (in_runner->initial_base * cRunner_Offset));

        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Out. Type=" << std::to_string(in_runner->out_type)
        << " Location=" << std::to_string(in_runner->out_location) << "\n";
    }
    else{
        in_runner->result_base = PowerPC::HostRead_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
    }

    if (PowerPC::HostRead_U8(aRunner_Stealing + (in_runner->initial_base * cRunner_Offset)) > in_runner->steal){
        in_runner->steal = PowerPC::HostRead_U8(aRunner_Stealing + (in_runner->initial_base * cRunner_Offset));
        std::cout << "Logging Runner " << std::to_string(in_runner->initial_base) << ": Steal. Type=" << std::to_string(in_runner->steal)<< "\n";
    }
}

std::string StatTracker::decode(std::string type, u8 value, bool decode){
    if (!decode) { return std::to_string(value);}

    std::string retVal = "Unable to Decode";
    
    if (type == "Character"){
        if (cCharIdToCharName.count(value)){
            retVal = cCharIdToCharName.at(value);
        }
    }
    else if (type == "Stadium"){
        if (cStadiumIdToStadiumName.count(value)){
            retVal = cStadiumIdToStadiumName.at(value);
        }
    }
    else if (type == "Contact"){
        if (cTypeOfContactToHR.count(value)){
            retVal = cTypeOfContactToHR.at(value);
        }
    }
    else if (type == "Hand"){
        if (cHandToHR.count(value)){
            retVal = cHandToHR.at(value);
        }
    }
    else if (type == "Stick"){
        if (cInputDirectionToHR.count(value)){
            retVal = cInputDirectionToHR.at(value);
        }
    }
    else if (type == "StickVec"){
        retVal = "";
        if ( (value & 0x1) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Left";
        }
        if ( (value & 0x2) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Right";
        } 
        if ( (value & 0x4) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Down";
        } 
        if ( (value & 0x8) > 0 ){
            if (retVal != ""){
                retVal += "+";
            }
            retVal += "Up";
        } 
    }
    else if (type == "Pitch"){
        if (cPitchTypeToHR.count(value)){
            retVal = cPitchTypeToHR.at(value);
        }
    }
    else if (type == "ChargePitch"){
        if (cChargePitchTypeToHR.count(value)){
            retVal = cChargePitchTypeToHR.at(value);
        }
    }
    else if (type == "Swing"){
        if (cTypeOfSwing.count(value)){
            retVal = cTypeOfSwing.at(value);
        }
    }
    else if (type == "Position"){
        if (cPosition.count(value)){
            retVal = cPosition.at(value);
        }
    }
    else if (type == "Action"){
        if (cFielderActions.count(value)){
            retVal = cFielderActions.at(value);
        }
    }
    else if (type == "Bobble"){
        if (cFielderBobbles.count(value)){
            retVal = cFielderBobbles.at(value);
        }
    }
    else if (type == "ManualSelect"){
        if (cManualSelectDecode.count(value)){
            retVal = cManualSelectDecode.at(value);
        }
    }
    else if (type == "Steal"){
        if (cStealType.count(value)){
            retVal = cStealType.at(value);
        }
    }
    else if (type == "Out"){
        if (cOutType.count(value)){
            retVal = cOutType.at(value);
        }
    }
    else if (type == "PrimaryContactResult"){
        if (cPrimaryContactResult.count(value)){
            retVal = cPrimaryContactResult.at(value);
        }
    }
    else if (type == "SecondaryContactResult"){
        if (cSecondaryContactResult.count(value)){
            retVal = cSecondaryContactResult.at(value);
        }
    }
    else if (type == "PitchResult"){
        if (cPitchResult.count(value)){
            retVal = cPitchResult.at(value);
        }
    }
    else if (type == "AtBatResult"){
        if (cAtBatResult.count(value)){
            retVal = cAtBatResult.at(value);
        }
    }
    else if (type == "QuitterTeam"){
        if (value == 0){
            retVal = "Home";
        }
        else if (value == 1){
            retVal = "Away";
        }
        else if (value == 2){
            retVal = "Crash";
        }
        else if (value == 0xFF){
            retVal = "None";
        }
    }
    else{
        retVal += ". Invalid Type (" + type + ")";
    }
    
    if (retVal == "Unable to Decode"){
        retVal += ". Invalid Value (" + std::to_string(value) + ").";
    }
    return ("\"" + retVal + "\"");
}

void StatTracker::postOngoingGame(Event& in_curr_event){
    if (!shouldSubmitGame()){ return; }

    std::cout << "postOngoingGame()\n";

    std::stringstream json_stream;

    json_stream << "{\n";
    std::string start_date_time = m_game_info.start_unix_date_time;
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Date - Start\": \"" << start_date_time << "\",\n";
    
    std::string tag_set_id_str = "\"\"";
    if (m_game_info.tag_set_id.has_value()){
        tag_set_id_str = std::to_string(m_game_info.tag_set_id.value());
    }
    json_stream << "  \"TagSetID\": " << tag_set_id_str << ",\n";
    json_stream << "  \"StadiumID\": " << decode("Stadium", m_game_info.stadium, false) << ",\n";
    json_stream << "  \"Away Player\": \""           << m_game_info.getAwayTeamPlayer().GetUserID() << "\",\n";
    json_stream << "  \"Home Player\": \""           << m_game_info.getHomeTeamPlayer().GetUserID() << "\",\n";

    u8 away_captain_roster_loc = (m_game_info.away_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;
    u8 home_captain_roster_loc = (m_game_info.home_port == m_game_info.team0_port) ? m_game_info.team0_captain_roster_loc : m_game_info.team1_captain_roster_loc;

    json_stream << "  \"Away Captain\": "            << std::to_string(away_captain_roster_loc) << ",\n";
    json_stream << "  \"Home Captain\": "            << std::to_string(home_captain_roster_loc) << ",\n";

    json_stream << "  \"Away Roster 0 CharID\": "            << std::to_string(m_game_info.character_summaries[0][0].char_id) << ",\n";
    json_stream << "  \"Away Roster 1 CharID\": "            << std::to_string(m_game_info.character_summaries[0][1].char_id) << ",\n";
    json_stream << "  \"Away Roster 2 CharID\": "            << std::to_string(m_game_info.character_summaries[0][2].char_id) << ",\n";
    json_stream << "  \"Away Roster 3 CharID\": "            << std::to_string(m_game_info.character_summaries[0][3].char_id) << ",\n";
    json_stream << "  \"Away Roster 4 CharID\": "            << std::to_string(m_game_info.character_summaries[0][4].char_id) << ",\n";
    json_stream << "  \"Away Roster 5 CharID\": "            << std::to_string(m_game_info.character_summaries[0][5].char_id) << ",\n";
    json_stream << "  \"Away Roster 6 CharID\": "            << std::to_string(m_game_info.character_summaries[0][6].char_id) << ",\n";
    json_stream << "  \"Away Roster 7 CharID\": "            << std::to_string(m_game_info.character_summaries[0][7].char_id) << ",\n";
    json_stream << "  \"Away Roster 8 CharID\": "            << std::to_string(m_game_info.character_summaries[0][8].char_id) << ",\n";
    json_stream << "  \"Home Roster 0 CharID\": "            << std::to_string(m_game_info.character_summaries[1][0].char_id) << ",\n";
    json_stream << "  \"Home Roster 1 CharID\": "            << std::to_string(m_game_info.character_summaries[1][1].char_id) << ",\n";
    json_stream << "  \"Home Roster 2 CharID\": "            << std::to_string(m_game_info.character_summaries[1][2].char_id) << ",\n";
    json_stream << "  \"Home Roster 3 CharID\": "            << std::to_string(m_game_info.character_summaries[1][3].char_id) << ",\n";
    json_stream << "  \"Home Roster 4 CharID\": "            << std::to_string(m_game_info.character_summaries[1][4].char_id) << ",\n";
    json_stream << "  \"Home Roster 5 CharID\": "            << std::to_string(m_game_info.character_summaries[1][5].char_id) << ",\n";
    json_stream << "  \"Home Roster 6 CharID\": "            << std::to_string(m_game_info.character_summaries[1][6].char_id) << ",\n";
    json_stream << "  \"Home Roster 7 CharID\": "            << std::to_string(m_game_info.character_summaries[1][7].char_id) << ",\n";
    json_stream << "  \"Home Roster 8 CharID\": "            << std::to_string(m_game_info.character_summaries[1][8].char_id) << ",\n";

    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << ",\n";
    json_stream << "  \"Pitcher\": "                 << std::to_string(in_curr_event.pitcher_roster_loc) << "\n";
    json_stream << "}\n";

    const Common::HttpRequest::Response response =
        m_http.Post("https://api.projectrio.app/populate_db/ongoing_game/", json_stream.str(),
            {
                {"Content-Type", "application/json"},
            }
        );
}
void StatTracker::updateOngoingGame(Event& in_curr_event){
    if (!shouldSubmitGame()){ return; }
    
    std::stringstream json_stream;

    json_stream << "{\n";
    json_stream << "  \"GameID\": \"" << m_game_info.game_id << "\",\n";
    json_stream << "  \"Inning\": "                  << std::to_string(in_curr_event.inning) << ",\n";
    json_stream << "  \"Half Inning\": "             << std::to_string(in_curr_event.half_inning) << ",\n";
    json_stream << "  \"Away Score\": "              << std::dec << in_curr_event.away_score << ",\n";
    json_stream << "  \"Home Score\": "              << std::dec << in_curr_event.home_score << ",\n";
    json_stream << "  \"Outs\": "                    << std::to_string(in_curr_event.outs) << ",\n";
    json_stream << "  \"Away Stars\": "              << std::to_string(in_curr_event.away_stars) << ",\n";
    json_stream << "  \"Home Stars\": "              << std::to_string(in_curr_event.home_stars) << ",\n";
    //json_stream << "  \"Chemistry Links on Base\": " << std::to_string(in_curr_event.chem_links_ob) << ",\n";
    json_stream << "  \"Pitcher\": "      << std::to_string(in_curr_event.pitcher_roster_loc) << ",\n";
    json_stream << "  \"Batter\": "       << std::to_string(in_curr_event.batter_roster_loc) << ",\n";

    bool runner_1, runner_2, runner_3;
    runner_1 = (in_curr_event.runner_1.has_value());
    runner_2 = (in_curr_event.runner_2.has_value());
    runner_3 = (in_curr_event.runner_3.has_value());

    json_stream << "  \"Runner 1B\": "       << std::to_string(runner_1) << ",\n";
    json_stream << "  \"Runner 2B\": "       << std::to_string(runner_2) << ",\n";
    json_stream << "  \"Runner 3B\": "       << std::to_string(runner_3) << "\n";
    json_stream << "}\n";

    const Common::HttpRequest::Response response =
        m_http.Post("https://api.projectrio.app/populate_db/ongoing_game/", json_stream.str(),
            {
                {"Content-Type", "application/json"},
            }
        );
}
