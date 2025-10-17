#pragma once

// ==================== 인증 패킷 ====================
// 클라이언트 -> 서버
#define PKT_CHECK_ID              "CHECK_ID"           // CHECK_ID|id
#define PKT_SIGNUP                "SIGNUP"             // SIGNUP|id|password|nickname
#define PKT_LOGIN                 "LOGIN"              // LOGIN|id|password
#define PKT_EDIT_NICK             "EDIT_NICK"          // EDIT_NICK|token|newNickname

// 서버 -> 클라이언트
#define PKT_ID_TAKEN              "ID_TAKEN"           // 아이디 중복
#define PKT_ID_AVAILABLE          "ID_AVAILABLE"       // 아이디 사용 가능
#define PKT_SIGNUP_OK             "SIGNUP_OK"          // SIGNUP_OK|token
#define PKT_SIGNUP_ERROR          "SIGNUP_ERROR"       // 회원가입 실패
#define PKT_LOGIN_OK              "LOGIN_OK"           // LOGIN_OK|token
#define PKT_LOGIN_NO_ACCOUNT      "LOGIN_NO_ACCOUNT"   // 계정 없음
#define PKT_LOGIN_WRONG_PW        "LOGIN_WRONG_PW"     // 비밀번호 불일치
#define PKT_LOGIN_SUSPENDED       "LOGIN_SUSPENDED"    // 정지된 계정
#define PKT_LOGIN_ERROR           "LOGIN_ERROR"        // 기타 로그인 오류
#define PKT_NICKNAME_EDIT_OK      "NICKNAME_EDIT_OK"   // 닉네임 수정 성공
#define PKT_NICK_DUPLICATE        "NICK_DUPLICATE"     // 닉네임 중복
#define PKT_NICKNAME_EDIT_ERROR   "NICKNAME_EDIT_ERROR" // 닉네임 수정 실패

// ==================== 사용자 정보 패킷  ====================
// 클라이언트 -> 서버
#define PKT_TOKEN                 "TOKEN"              // TOKEN|token (토큰으로 사용자 정보 조회)
#define PKT_CHECK_NICKNAME        "CHECK_NICKNAME"     // CHECK_NICKNAME|nickname

// 서버 -> 클라이언트
#define PKT_NICKNAME              "NICKNAME"           // NICKNAME|nickname|WINS|wins|LOSSES|losses
#define PKT_INVALID_TOKEN         "INVALID_TOKEN"      // 토큰 무효
#define PKT_NICKNAME_TAKEN        "NICKNAME_TAKEN"     // 닉네임 중복
#define PKT_NICKNAME_AVAILABLE    "NICKNAME_AVAILABLE" // 닉네임 사용 가능

// ==================== 게임 관련 패킷 ====================
// 클라이언트 -> 서버
#define PKT_CMD_QUERY_WAIT        "CMD|QUERY_WAIT"     // CMD|QUERY_WAIT|token (게임 매칭 대기 시작)
#define PKT_MATCHING_CANCEL       "MATCHING_CANCEL"    // MATCHING_CANCEL|token (게임 매칭 취소)
#define PKT_SESSION_READY         "SESSION_READY"      // SESSION_READY|token (게임 세션 준비)
#define PKT_READY_TO_GO           "READY_TO_GO"        // READY_TO_GO (게임 시작 준비 완료)
#define PKT_GET_ROLE              "GET_ROLE"           // GET_ROLE (자신의 역할 조회)
#define PKT_GET_ALL_CARDS         "GET_ALL_CARDS"      // GET_ALL_CARDS (모든 카드 조회)
#define PKT_HINT                  "HINT"               // HINT|word|number (힌트 제공)
#define PKT_ANSWER                "ANSWER"             // ANSWER|word (카드 선택/정답)
#define PKT_CHAT                  "CHAT"               // CHAT|message (채팅)
#define PKT_GAME_QUIT             "GAME_QUIT"          // GAME_QUIT (게임 종료)

// 서버 -> 클라이언트
#define PKT_WAIT_REPLY            "WAIT_REPLY"         // WAIT_REPLY|playerCount (매칭 대기 중)
#define PKT_QUEUE_FULL            "QUEUE_FULL"         // QUEUE_FULL (매칭 인원 충족, 게임 시작)
#define PKT_CANCEL_OK             "CANCEL_OK"          // CANCEL_OK (매칭 취소 완료)
#define PKT_SESSION_ACK           "SESSION_ACK"        // SESSION_ACK (세션 준비 완료)
#define PKT_SESSION_NOT_FOUND     "SESSION_NOT_FOUND"  // SESSION_NOT_FOUND (세션 없음)
#define PKT_GAME_INIT             "GAME_INIT"          // GAME_INIT|nick1|role1|team1|leader1|nick2|role2|team2|leader2|... (게임 초기화)
#define PKT_GAME_START            "GAME_START"         // GAME_START|sessionId (게임 시작)
#define PKT_GAME_FAILED           "GAME_FAILED"        // GAME_FAILED|reason (게임 시작 실패)
#define PKT_ALL_CARDS             "ALL_CARDS"          // ALL_CARDS|word1|type1|isUsed1|word2|type2|isUsed2|... (모든 카드 정보)
#define PKT_ROLE_INFO             "ROLE_INFO"          // ROLE_INFO|roleNumber (플레이어 역할: 0-3)
#define PKT_TURN_UPDATE           "TURN_UPDATE"        // TURN_UPDATE|team|phase|redScore|blueScore (턴 정보)
#define PKT_HINT_MSG              "HINT"               // HINT|team|word|count (힌트 전송)
#define PKT_CARD_UPDATE           "CARD_UPDATE"        // CARD_UPDATE|cardIndex|isUsed (카드 상태 업데이트)
#define PKT_CHAT_MSG              "CHAT"               // CHAT|team|roleNum|nickname|message (채팅 메시지)
#define PKT_EVENT_QUEUE_FULL      "EVENT_QUEUE_FULL"   // EVENT_QUEUE_FULL (이벤트 큐 가득참)
#define PKT_REPORT                "REPORT"             // REPORT|token|targetNickname (사용자 신고)
#define PKT_REPORT_OK             "REPORT_OK"          // REPORT_OK|reportCount[|SUSPENDED] (신고 완료)
#define PKT_REPORT_ERROR          "REPORT_ERROR"       // REPORT_ERROR|reason (신고 실패)

// ==================== 기타 패킷 ====================
#define PKT_ERROR                 "ERROR"              // 기타 오류

