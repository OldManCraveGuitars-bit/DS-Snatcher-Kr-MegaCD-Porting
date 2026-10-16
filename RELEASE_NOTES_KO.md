# v0.1Demo

**버그가 남아 있는 개발 중 데모이며, 일부 구간만 플레이 가능합니다. 전체 게임을 끝까지 진행할 수 없습니다.**

**v0.1Demo 패처는 1회성 배포입니다. 이후 배포 버전이 나올 때까지 패처 업데이트를 제공하지 않습니다.**

Nintendo DS용 한국어 SNATCHER 데모입니다. 한글 패치 v0.775 기준 대사·보이스 자막과 DS 두 화면·터치 조작을 제공합니다.

플레이 범위는 두 오프닝, 초기 본부·사격 연습, 터보사이클 이동, 폐공장 조사·전투·탈출·본부 복귀의 주요 경로입니다. **이후 이야기·엔딩·모든 분기의 정상 진행은 지원하지 않습니다.** BGM·효과음·보이스의 끊김과 늘어짐, 구간별 속도 저하가 남아 있습니다.

## 다운로드

`DS-Snatcher-Kr-MegaCD-Porting-v0.1Demo-Patcher.zip`을 받고 압축을 푸십시오. Windows 64비트 Python 3.12 이상에서 requirements.txt를 설치한 다음 `Patch-v0.1Demo.cmd`를 실행합니다. 본인 소유의 깨끗한 미국판 CUE/BIN과 지원 USA BIOS가 필요합니다. C/C++ 컴파일러는 필요하지 않습니다.

[패치 방법과 준비물](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/main/docs/PATCHING_KO.md) · [알려진 문제](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/main/docs/KNOWN_ISSUES_KO.md) · [스크린샷](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting#화면)

패처는 원본 리소스 203개를 생성·검증하고 NDS를 조립합니다. 생성된 NDS는 121,069,568바이트이며 SHA-256은 `0578ba7b425664ac5726d4e85f934dc9cf943d3bc6a9ab52bb2ce01667f42bd1`입니다. 게임 원본·BIOS·완성 NDS는 포함하지 않습니다.

에뮬레이터에서 오프닝·본부·사격 연습·폐공장 전투·탈출·복귀·저장/불러오기를 검사했습니다. 실제 DS 전 기종의 성능이나 전편 완성을 보장하는 검사는 아닙니다.

## 실제 화면

| 접수처 | 국장실 |
| --- | --- |
| ![접수처](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.1Demo/screenshots/reception.png) | ![국장실](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.1Demo/screenshots/chief-office.png) |

| 사격 연습 | 폐공장 전투 |
| --- | --- |
| ![사격 연습](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.1Demo/screenshots/shooting-practice.png) | ![폐공장 전투](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.1Demo/screenshots/factory-battle.png) |
