#ifndef _web_server_defined
#define _web_server_defined

const char* HEADER_BEGIN = "<!DOCTYPE html><html><head><title>ARTNODE</title><style>";

const char* STYLE = "* {             margin: 0;             padding: 0;             }             html, body{                 background-color: #363636;                 height: 100%%;             }             h1,h2,h3{                 color: #FFFFFF;             }             button{                 height: 75px;                 width: 200px;                 float: left;             }";

const char* STYLEID = "#txtw{                 color: #FFFFFF;             }             #tcntr {                 text-align: center             }             #ccntr {                 align-content: center             }                          #btn_on {                 color: #FFFFFF;                 font-size: 18px;                 background-color: #22FF0077;                 width: 50%%;             }       #btn_off {                 color: #FFFFFF;                 font-size: 18px;                 background-color: #FF220077;                 width: 50%%;             }      #btn_nav{                 color: #FFFFFF;                 font-size: 18px;                 background-color: #555555;                 width: 12.5%%;             }             #btn_nav_hi{                 color: #FFFFFF;                 font-size: 18px;                 background-color: #555555;                 width: 12.5%%;                 border: 3px solid #FFDD11;             }#btn_sys{                 color: #FFFFFF;                 font-size: 18px;                 background-color: #444444;                 width: 12.5%%;             }             #btn_sys_hi{                 color: #FFFFFF;                 font-size: 18px;                 background-color: #444444;                 width: 12.5%%;                 border: 2px solid #FFDD11;             }#div_nav{                 float: left;             } #btn_txt{color: #111111;font-size: 18px;background-color: #DDDDDD;width: 12.5%%;}";

const char* STYLECLASS = ".wrapper {                 position: relative;                 min-height: 100%%;             }             .content {                 padding-bottom: 90px;                 padding-top: 3px;             }             .footer {                 position: absolute;                 left: 0;                 bottom: 0;                 width: 100%%;                 height: 80px;             }";

const char* HEADER_END = "</style></head><body>";

const char* CSS_SETTINGS = "* {    margin : 0;padding : 0;color : #FFFFFF;}html, body{    background-color : #363636;height:    100 %% ;} input{    color: #111111;    text-align: center;} button{height:    75px;    font-size : 18px;width:    200px;    float : left;}table{    text-align : center;width:    100 %% ;    max-width : 480px;margin:    0 auto;}tr{height:    28px;} td{ width:50 %% ;}";

const char* CSS_SETTINGS_ID = "#tcntr {    text-align: center}#ccntr {    align-content: center}#txtdot{    font-size: 20px;}#tdr{    min-width: 250px;}";

const char* CSS_SETTINGS_CLASS = ".btn_sys{    background-color: #444444;}.btn_nav{    background-color: #555555;}.btn_on {    background-color: #22FF0077;    }.btn_off {    background-color: #FF220077;}.higlt{    border: 2px solid #FFDD11;} .wrapper {    position: relative;    min-height: 100%%;}.content {    padding-bottom: 90px;    padding-top: 3px;}.footer {    position: relative;    left: 0;    bottom: 0;    width: 100%%;    height: 70px;}";

const char* CSS_APPLY = "* {margin: 0;padding: 0;color: #FFFFFF;}html, body{    background-color: #363636;    height: 100%%;}h1,h2,h3,h4{    color: #FFFFFF;}button{    height: 75px;    width: 200px;    font-size: 18px;    float: left;}table{    text-align: center;    width: 100%%;    max-width: 480px;    margin: 0 auto;}tr{    height: 28px;}td{    width: 50%%;}";

const char* CSS_APPLY_ID = "#tcntr {    text-align: center}#ccntr {    align-content: center}#txtdot{    font-size: 20px;}#tdr{    min-width: 250px;}";

const char* CSS_APPLY_CLASS = ".btn_sys{    background-color: #444444;}.btn_nav{    background-color: #555555;}.btn_on {    background-color: #22FF0077;    }.btn_off {    background-color: #FF220077;}.higlt{    border: 2px solid #FFDD11;}.sw_50{    width: 50%%;}.wrapper {    position: relative;    min-height: 100%%;}.content {    padding-bottom: 90px;    padding-top: 3px;}.footer {    position: absolute;    left: 0;    bottom: 0;    width: 100%%;    height: 80px;}";

#endif
