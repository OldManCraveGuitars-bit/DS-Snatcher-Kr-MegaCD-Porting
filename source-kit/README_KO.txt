SNATCHER DS 비공식 한글화·포팅 v1.01 정식 버전

한글화 및 포팅: 기타 깎는 노인
Mega-CD판 SNATCHER 한글패치의 최종 검수 번역을 기반으로 제작한 Nintendo DS용 비공식 이식입니다.
ACT 1~3와 엔딩을 포함하며, 두 화면 배치·한국어 자막·터치 조작과 게임 내부 저장을 지원합니다.
KONAMI가 제작하거나 승인한 공식 제품이 아닙니다.

Windows EXE에서 수정되지 않은 북미판 원본 CUE/BIN 전체, 지원 BIOS, 새 PC 저장 위치를 선택하세요.
최종 한글 번역은 자동 적용됩니다. 원본 게임·BIOS·완성 NDS는 배포에 포함하지 않습니다.
기존 snatcher_ds 저장 폴더 전체를 백업하고 유지한 채 NDS를 교체하세요.

소스 패처: Python 3.13 64비트와 requirements.txt를 설치한 뒤
python -X utf8 -B patcher/patch_release.py --source "원본.cue" --bios "USA.bin" --output "새 결과 폴더"
또는 Patch-development.cmd를 실행하세요. 1,230개 리소스와 생성 NDS를 검사합니다.
지원 BIOS SHA-256: b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4

v1.01 수정: 폐공장 앞 소지품 → 보기 → 정커 신분증 선택 시 PC16AC OP42 진행 중단 수정.
신분증 화면 저장·불러오기 및 폐공장 복귀를 지원합니다.

v1.0 누적 변경: 최종 한글 대사·누락 자막, 아우터헤븐 암호 선택 뒤 FM 잡음 수정,
ACT3 전투와 화면 이동·페이드 처리 최적화, 초상화 아래 LIFE와 중앙 체력바,
ACT3 본부 세이브 위치, 엔딩 문구·한글화 및 포팅 크레딧, 사운드 체크 메뉴 제거.
배경음·효과음·보이스 개별 설정과 손전등 십자키/A·터치 조작을 유지합니다.

일반 DS 4 MiB 기준으로 검사합니다. 게임 내부 저장에는 홈브루 SD/DLDI 읽기·쓰기가 필요합니다.
최종판의 모든 분기와 모든 실기를 다시 완주한 것은 아닙니다. 정확한 검사 범위는 저장소 docs/RELEASE_STATUS.md를 참고하세요.
TV 필터·스캔라인은 기본 Off이며, 전투 경량 표시 경로는 두 옵션이 Off일 때 사용합니다.

플레이어 재빌드: REBUILD-PLAYER_KO.txt / 변환 DLL 재빌드: REBUILD-CONVERTERS_KO.txt
실행 파일 고지 보존: REBUILD-NOTICES_KO.txt
원본 의존 표·그래프·미번역 원문은 사용자의 디스크에서 복원합니다.
실행 아이콘의 제작 기록은 art/launcher/에 있으며 각 구성 요소의 라이선스·고지를 보존합니다.
