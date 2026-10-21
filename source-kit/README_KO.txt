SNATCHER DS v0.9 — 전체 테스트용 사전 릴리스

Mega-CD판 SNATCHER 한글패치 V0.9의 번역과 자막을 바탕으로 만든 비공식 DS 이식입니다.
완성판이 아닙니다. 버그, 일부 구간·선택 분기의 진행 불가와 사운드 끊김이 남아 있을 수 있습니다.
ACT 1~3가 시험 대상이며 모든 분기·실기 조합을 검증한 것은 아닙니다.

일반 사용자는 동봉 Windows EXE에서 원본 북미판 CUE/BIN 전체와 지원 BIOS를 선택하세요.
한글패치를 먼저 적용하지 않은 원본을 사용합니다. 한국어 V0.9는 자동으로 적용됩니다.
기존 snatcher_ds 저장 폴더는 백업하고 유지하세요. 원본 게임·BIOS·완성 NDS는 포함하지 않습니다.

소스 패처: Python 3.13 64비트와 requirements.txt를 설치한 뒤
python -X utf8 -B patcher/patch_release.py --source "원본.cue" --bios "USA.bin" --output "새 결과 폴더"
또는 Patch-development.cmd를 실행하세요. 모든 1,230개 리소스의 길이와 해시가 일치해야 완료됩니다.
지원 BIOS SHA-256: b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4

플레이어 소스 복원·재빌드: REBUILD-PLAYER_KO.txt
변환 DLL 재빌드: REBUILD-CONVERTERS_KO.txt
실행 파일 고지 보존: REBUILD-NOTICES_KO.txt
원본 의존 표·그래프·미번역 원문은 사용자의 디스크에서 복원합니다.
신규 실행 아이콘은 내장 이미지 생성 도구로 제작해 DS 32×32·16색으로 변환했습니다.
대응 프롬프트와 변환 기록은 art/launcher/에 있습니다.

v0.9: 한글 V0.9, 실제 전투 LIFE/체력바 한 줄 통일, ACT1 END 순간 화면 교정,
ACT1·2 클리어 세이브 장소 공란, BGM/효과음/보이스 개별 On/Off 및 새 실행 아이콘.
보이스를 꺼도 자막과 원래 진행 타이밍은 유지합니다. 실기 성능을 보장하는 검사는 아닙니다.
전체 검증 범위와 잔여 문제는 저장소 docs/RELEASE_STATUS.md, docs/KNOWN_ISSUES_KO.md를 따릅니다.
기존 v0.1Demo는 변경하지 않는 1회성 데모이며 이번 v0.9는 별도 전체 테스트용 배포입니다.
KONAMI 등 원 권리자 및 각 외부 구성 요소의 라이선스·고지는 보존됩니다.
