# v0.81 — 전체 테스트용

![SNATCHER DS 로고](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.81/screenshots/title.png)

> **v0.81은 전체 테스트용 버전입니다. 완성판이 아니며, 버그·일부 구간의 진행 불가·사운드 끊김이 남아 있을 수 있습니다.**
>
> **ACT 1~3 전체 스토리를 대상으로 시험하는 버전이며, 모든 분기·실기 동작을 보장하는 완성판이 아닙니다.**

**이 프로젝트는 [Mega-CD판 SNATCHER 한글패치](https://github.com/OldManCraveGuitars-bit/MegaCD-Snatcher-Kr)를 기반으로 제작한 Nintendo DS용 비공식 이식입니다. 한국어 대사와 보이스 자막은 해당 한글패치 0.81 자료를 반영했습니다.**

## 받기와 실행

**일반 사용자는 `Snatcher-DS-v0.81-Patcher.exe` 또는 `DS-Snatcher-Kr-MegaCD-Porting-v0.81-Patcher.zip`을 받으세요.** Windows 10/11 x64용이며 Python 설치가 필요 없습니다.

1. EXE 실행 → **원본 북미판 ROM(.cue)** 선택. CUE가 참조하는 모든 BIN도 필요합니다.
2. 지원 **Sega CD USA BIOS**와 **PC 저장 위치** 선택.
3. **NDS 만들기** → 완료 후 결과 폴더의 `snatcher_ds.nds`를 SD 카드로 복사.

한글 0.81은 자동 적용됩니다. 이미 한글패치한 ROM을 넣지 마세요. 기존 `snatcher_ds` 저장 폴더를 백업하세요. [지원 BIOS·패치 방법](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/main/docs/PATCHING_KO.md)

## 입력 원본 ROM · BIOS 해시

**북미판 원본 ROM — `Snatcher (USA).bin` (단일 BIN 기준, 625,192,176바이트)**

```text
SHA-256: 8d6f4dba8d9ce59991be9f65635047adc3b0f37ea2358f8c48b251883c9e55b6
```

**지원 Sega CD USA BIOS — `us_scd2_9306.bin` (131,072바이트)**

```text
SHA-256: b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4
```

위 값은 **패처에 넣을 원본 파일**의 해시입니다. 패처에서는 연결된 **CUE**를 선택하세요. 여러 BIN 트랙으로 나뉜 원본은 각 파일의 해시가 위 단일 BIN 해시와 다르며 직접 비교할 수 없습니다. 첨부된 `INPUT-SHA256-v0.81.txt`에서도 확인할 수 있습니다.

## 변경과 확인 범위

- Mega-CD판 한글패치 0.81 자료 반영, ACT 1~3·엔딩 주요 경로 연결.
- BGM 계산 부하 완화, 상단 그림 비율·하단 초상화 배치 정리, 터치 조작과 5개 저장 슬롯, 클리어 후 숨겨진 ACT 선택.
- ROM·BIOS 파일을 고르는 Windows 단독 EXE 패처와 설명서·라이선스·대응 소스 제공.
- 실제 EXE로 1,230개 리소스와 NDS 생성 검증. 같은 NDS의 주요 경로 34구간 / 1,006,240 디스플레이 프레임, 엔딩과 재시작 후 클리어 기록 확인.

**버그·일부 구간/선택 분기의 진행 불가·특정 장면 사운드 끊김 가능성이 남아 있습니다.** PC 에뮬레이터의 주요 경로 검사이며 모든 실기·선택 분기를 검증한 것은 아닙니다. [알려진 문제](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.81/docs/KNOWN_ISSUES_KO.md) · [검증 기록](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.81/docs/RELEASE_STATUS.md)

## 실제 화면

| 접수처 | 보이스 대화 |
| --- | --- |
| ![접수처](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.81/screenshots/reception.png) | ![보이스 대화](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.81/screenshots/voice.png) |

| 터치 자판 | 플라토 거리 |
| --- | --- |
| ![터치 자판](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.81/screenshots/keyboard.png) | ![플라토 거리](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.81/screenshots/plato.png) |

모두 동일 검증 NDS의 편집하지 않은 에뮬레이터 캡처입니다. 원본 게임·BIOS·완성 NDS는 배포하지 않습니다. 기존 v0.1Demo는 이전 배포로 보존합니다.
