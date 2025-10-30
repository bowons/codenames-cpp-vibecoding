#pragma once

// --- Lobby / Matching / Session ---
#define PKT_CMD_QUERY_WAIT          "CMD|QUERY_WAIT"         // client -> server: CMD|QUERY_WAIT|token
#define PKT_WAIT_REPLY              "WAIT_REPLY"             // server -> client: WAIT_REPLY|playerCount|maxPlayers
#define PKT_QUEUE_FULL              "QUEUE_FULL"             // server -> client: 대기 인원 충족
#define PKT_QUEUE_ERROR             "QUEUE_ERROR"            // server -> client: 큐 에러
#define PKT_INVALID_TOKEN           "INVALID_TOKEN"          // server -> client: 토큰 무효
#define PKT_SESSION_ACK            "SESSION_ACK"            // server -> client: 세션 준비 완료
#define PKT_SESSION_NOT_FOUND      "SESSION_NOT_FOUND"      // server -> client: 세션 없음
#define PKT_CANCEL_OK              "CANCEL_OK"              // server -> client: 매칭 취소 완료
#define PKT_LOBBY_ERROR_UNKNOWN    "LOBBY_ERROR|UNKNOWN_PACKET" // server -> client: 알 수 없는 로비 패킷

// --- Auth ---
#define PKT_CHECK_ID               "CHECK_ID"               // client -> server: CHECK_ID|id
#define PKT_CHECK_ID_DUPLICATE     "CHECK_ID_DUPLICATE"     // server -> client
#define PKT_CHECK_ID_OK            "CHECK_ID_OK"            // server -> client
#define PKT_CHECK_ID_ERROR         "CHECK_ID_ERROR"         // server -> client

#define PKT_SIGNUP                 "SIGNUP"                 // client -> server: SIGNUP|id|pw|nickname
#define PKT_SIGNUP_OK              "SIGNUP_OK"              // server -> client: SIGNUP_OK|token
#define PKT_SIGNUP_DUPLICATE       "SIGNUP_DUPLICATE"       // server -> client
#define PKT_SIGNUP_ERROR           "SIGNUP_ERROR"           // server -> client

#define PKT_LOGIN                  "LOGIN"                  // client -> server: LOGIN|id|pw
#define PKT_LOGIN_OK               "LOGIN_OK"               // server -> client: LOGIN_OK|token
#define PKT_LOGIN_NO_ACCOUNT       "LOGIN_NO_ACCOUNT"       // server -> client
#define PKT_LOGIN_WRONG_PW         "LOGIN_WRONG_PW"         // server -> client
#define PKT_LOGIN_SUSPENDED        "LOGIN_SUSPENDED"        // server -> client
#define PKT_LOGIN_ERROR            "LOGIN_ERROR"            // server -> client

#define PKT_TOKEN                  "TOKEN"                  // client -> server: TOKEN|token
#define PKT_TOKEN_VALID            "TOKEN_VALID"            // server -> client: TOKEN_VALID|nickname

#define PKT_EDIT_NICK              "EDIT_NICK"              // client -> server: EDIT_NICK|token|newNickname
#define PKT_NICKNAME_EDIT_OK       "NICKNAME_EDIT_OK"       // server -> client
#define PKT_NICKNAME_EDIT_ERROR    "NICKNAME_EDIT_ERROR"    // server -> client

#define PKT_AUTH_ERROR             "AUTH_ERROR"              // server -> client: AUTH_ERROR|reason (reason is sent after '|')

// --- Game (GameManager) ---
#define PKT_GAME_INIT              "GAME_INIT"              // server -> client: GAME_INIT|nick1|role1|team1|leader1|...
#define PKT_ALL_CARDS              "ALL_CARDS"              // server -> client: ALL_CARDS|word|type|isUsed|...
#define PKT_CARD_UPDATE            "CARD_UPDATE"            // server -> client: CARD_UPDATE|cardIndex|isUsed
#define PKT_TURN_UPDATE            "TURN_UPDATE"            // server -> client: TURN_UPDATE|team|phase|redScore|blueScore
#define PKT_HINT_MSG               "HINT"                   // server -> client: HINT|team|word|count
#define PKT_CHAT                  "CHAT"                   // server -> client: CHAT|team|roleNum|nickname|message
#define PKT_ANSWER                 "ANSWER"                 // client -> server: ANSWER|word
#define PKT_ANSWER_RESULT          "ANSWER_RESULT"          // server -> client: ANSWER_RESULT|result|word (ex: ANSWER_RESULT|INVALID|word)
#define PKT_GAME_OVER              "GAME_OVER"              // server -> client: GAME_OVER|winner
#define PKT_GAME_NOT_IMPLEMENTED   "GAME_NOT_IMPLEMENTED"   // server internal response
// 메시지 내부 토큰
#define PKT_SYSTEM                 "SYSTEM"                 // system 채널 등에서 사용되는 토큰
#define PKT_EMPTY                  "EMPTY"                  // GAME_INIT 등에서 빈 슬롯 표기

// --- Server control / errors ---
#define PKT_GAME_CREATE_ERROR      "GAME_CREATE_ERROR"      // server -> client: 게임룸 생성 실패
#define PKT_GAME_ERROR             "GAME_ERROR"             // generic game error (placeholder)

// --- Additional client->server commands (defined server-side for consistency) ---
// these are commands that clients may send and server recognizes
#define PKT_MATCHING_CANCEL        "MATCHING_CANCEL"        // client -> server: MATCHING_CANCEL|token
#define PKT_SESSION_READY          "SESSION_READY"         // client -> server: SESSION_READY|token
#define PKT_READY_TO_GO            "READY_TO_GO"           // client -> server: READY_TO_GO
#define PKT_GET_ROLE               "GET_ROLE"              // client -> server: GET_ROLE
#define PKT_GET_ALL_CARDS          "GET_ALL_CARDS"         // client -> server: GET_ALL_CARDS
#define PKT_GAME_START             "GAME_START"            // server -> client: GAME_START|sessionId
#define PKT_GAME_FAILED            "GAME_FAILED"           // server -> client: GAME_FAILED|reason
#define PKT_ROLE_INFO              "ROLE_INFO"             // server -> client: ROLE_INFO|roleNumber
#define PKT_CHAT_MSG               "CHAT"                  // server -> client / client -> server chat format
#define PKT_EVENT_QUEUE_FULL       "EVENT_QUEUE_FULL"      // server -> client: event queue full
#define PKT_REPORT                 "REPORT"                // REPORT|token|targetNickname
#define PKT_REPORT_OK              "REPORT_OK"             // REPORT_OK|reportCount[|SUSPENDED]
#define PKT_REPORT_ERROR           "REPORT_ERROR"          // REPORT_ERROR|reason
#define PKT_ERROR                  "ERROR"                 // generic error token

// 파일 끝

