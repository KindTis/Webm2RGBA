# Webm2RGBA

개발환경: vs2017  
사용언어: c++  


WebM Prject의 libvpx와 libwebm 라이브러리를 이용하여 webm 영상을 디코딩하여 RGBA로 변환 해주는 예제입니다.  
libwebm을 수정하여, webm의 알파 채널 정보를 디코딩 할수 있습니다.  
yuv -> rgb의 변환에 sse2 명령어 셋을 사용 하였습니다.  

origin libwebm : https://github.com/webmproject/libwebm  
modified libwem to decode alpha transparency : https://github.com/KindTis/libwebm  
yuv to rgb via sse2 : https://github.com/descampsa/yuv2rgb  
