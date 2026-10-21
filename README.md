# DS-Snatcher-Kr-MegaCD-Porting · v0.9

<p align="center"><img src="screenshots/title.png" alt="SNATCHER DS 로고" width="256" height="384"></p>

> **V0.9는 전체 테스트용 사전 릴리스입니다. 완성판이 아닙니다. 버그와 일부 구간·선택 분기의 진행 불가, 사운드 끊김이 남아 있을 수 있습니다.**
>
> ACT 1~3 전체 스토리 범위를 시험하는 버전이며, 모든 분기와 실기 조합의 정상 동작을 보장하지 않습니다.

**[Mega-CD판 SNATCHER 한글패치](https://github.com/OldManCraveGuitars-bit/MegaCD-Snatcher-Kr)를 기반으로 제작한 Nintendo DS용 비공식 이식입니다. 이번 버전은 원본 한글패치 V0.9의 번역·보이스 자막을 반영했습니다.**

상단은 장면 그림, 하단은 대사·초상화·터치 선택지와 조작으로 구성했습니다. 원본 게임·BIOS·완성 NDS는 배포하지 않으며, 패처가 사용자가 보유한 원본으로 NDS를 생성합니다.

## 다운로드

- **[Windows EXE 패처](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/releases/download/v0.9/Snatcher-DS-v0.9-Patcher.exe)** — Python 설치 없이 실행.
- **[패처·사용 방법·라이선스·대응 소스 ZIP](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/releases/download/v0.9/DS-Snatcher-Kr-MegaCD-Porting-v0.9-Patcher.zip)**
- [V0.9 릴리스](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/releases/tag/v0.9) · [배포 파일 SHA-256](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.9/SHA256SUMS-v0.9.txt) · [입력 ROM·BIOS SHA-256](INPUT-SHA256-v0.9.txt)

## V0.9 변경점

- Mega-CD 한글패치 **V0.9** 반영: 일반 대사 4개, 보이스 22개의 변경 자막 27쪽, ACT2 회상 변경 자막 30쪽. 기존 페이지 번호와 음성 표시 타이밍을 유지했습니다.
- 첫 폐공장 전투를 포함한 실제 전투의 체력 표시 통일: 상단 초상화 아래에 **LIFE와 체력바가 한 줄**로 나옵니다. 하단은 사격 조작에 사용합니다. 점수 방식의 사격 연습과 마지막 판단 구간은 원래 규칙을 유지합니다.
- ACT1 END 직전에 이전 그림의 하단이 순간적으로 보이던 전환 수정.
- ACT1·ACT2 클리어 저장의 잘못된 장소 표기를 공란으로 변경. ACT 표시는 유지합니다.
- 메뉴 설정에서 **배경음 / 효과음 / 보이스 On·Off**를 각각 선택. 보이스를 꺼도 자막과 진행 타이밍은 유지합니다.
- 푸른 기계 해골·붉은 눈의 새 **32×32·16색 실행 아이콘** 적용.
- v0.811의 NitroFS 실행 호환성 수정과 기존 사운드 최적화 유지.

## 패치 방법

1. Windows 10/11 x64 PC에서 EXE를 실행합니다.
2. 수정되지 않은 **북미판 원본 CUE**와 연결된 모든 BIN 트랙, 아래의 **지원 USA BIOS**, **PC 저장 위치**를 지정합니다.
3. **NDS 만들기**를 누릅니다. 한글 V0.9는 자동 적용되며 변환에 수 분이 걸립니다.
4. 완료 후 결과 폴더의 `snatcher_ds.nds`를 SD 카드에 복사합니다. 기존 `snatcher_ds` 저장 폴더는 백업하고 보존하세요.

이미 한글패치한 ROM을 입력하지 않습니다. 원본 파일과 기존 출력 폴더는 덮어쓰지 않습니다. CHD·ZIP·BIN 단독 입력은 지원하지 않습니다. [상세 방법](docs/PATCHING_KO.md)

## 원본 ROM · BIOS 해시

단일 BIN 북미판 `Snatcher (USA).bin` — 625,192,176바이트:

```text
8d6f4dba8d9ce59991be9f65635047adc3b0f37ea2358f8c48b251883c9e55b6
```

지원 USA BIOS `us_scd2_9306.bin` — 131,072바이트:

```text
b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4
```

이는 **입력 원본의 SHA-256**입니다. 여러 BIN으로 나뉜 원본은 개별 파일 해시를 단일 BIN 값과 직접 비교할 수 없습니다. 패처에는 연결된 CUE를 지정합니다. BIOS는 위 해시와 일치하는 파일을 사용해야 합니다.

## 화면

| 첫 전투 · 통일한 LIFE 표시 | ACT2 회상 · V0.9 자막 |
| --- | --- |
| ![전투](screenshots/combat-v09.png) | ![회상 자막](screenshots/recap-v09.png) |

| 접수처 · 오프닝 후 첫 대사 | 보이스 대화 |
| --- | --- |
| ![접수처](screenshots/reception.png) | ![대화](screenshots/voice.png) |

![실행 아이콘 확대](screenshots/launcher-v09.png)

게임 화면은 에뮬레이터의 256×384 캡처입니다. 실행 아이콘만 확인하기 쉽게 정수 배율로 확대했습니다. 아이콘은 이미지 생성 도구로 제작한 뒤 DS의 16색 규격으로 변환했습니다. 각 캡처의 버전·검사 빌드 해시는 [기록](screenshots/manifest.json)에 구분했습니다.

## 호환성과 검증 범위

일반 DS/DS Lite의 4 MiB 메모리를 기준으로 개발합니다. 게임 내부 저장에는 홈브루 SD/DLDI 읽기·쓰기가 필요합니다. **melonDS DS와 구형 RetroArch melonDS는 다른 코어입니다.** 검사한 구형 melonDS 0.9.3은 홈브루 SD를 제공하지 않아 내부 저장·불러오기를 사용할 수 없습니다. DSi SD 옵션과는 별개입니다.

기존 V0.81에서 ACT 1~3 주요 경로와 엔딩을 연속 검사했습니다. V0.9에서는 변경된 전투 화면, ACT1 종료·클리어 저장·ACT2 재개, 회상 자막, 패처 결과와 실행 호환성을 별도로 검사했습니다. **V0.9의 모든 분기를 새 게임부터 다시 완주한 것은 아닙니다.** R4 제품·커널별 실기 및 Android APK의 최신 수정본은 별도 확인이 필요합니다.

- [검증 범위](docs/RELEASE_STATUS.md) · [알려진 문제](docs/KNOWN_ISSUES_KO.md)
- [조작과 저장](docs/PLAYING_KO.md) · [소스 재빌드](docs/BUILDING.md)
- [외부 구성 요소와 권리 표기](THIRD_PARTY_NOTICES.md)

이전 V0.1Demo는 변경하지 않는 1회성 데모입니다. V0.9는 별도 전체 테스트용 배포입니다. SNATCHER 원작의 권리는 KONAMI 등 해당 권리자에게 있습니다.
