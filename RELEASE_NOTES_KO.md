# v0.811 — 전체 테스트용

![SNATCHER DS 로고](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.811/screenshots/title.png)

> **v0.811은 전체 테스트용 버전입니다. 완성판이 아니며, 버그·일부 구간의 진행 불가·사운드 끊김이 남아 있을 수 있습니다.**
>
> **ACT 1~3 전체 스토리를 대상으로 시험하는 버전이며, 모든 분기·실기 동작을 보장하는 완성판이 아닙니다.**

**이 프로젝트는 [Mega-CD판 SNATCHER 한글패치](https://github.com/OldManCraveGuitars-bit/MegaCD-Snatcher-Kr)를 기반으로 제작한 Nintendo DS용 비공식 이식입니다. 한국어 대사와 보이스 자막은 해당 한글패치 0.81 자료를 반영했습니다.**

## v0.811 실행 호환성 수정

일부 실행 환경에서 시작하자마자 **“오프닝을 읽을 수 없습니다”**가 표시되던 NDS 조립 오류를 수정했습니다. 패처가 빠뜨렸던 `NitroFS!` 표식을 넣어, SD 파일 경로 없이 카트리지에서 리소스를 읽는 경로도 사용할 수 있게 했습니다.

- 구형 RetroArch **melonDS**와 **DeSmuME**의 시작 오류를 재현했고, 새 EXE로 만든 NDS가 오프닝 1·2를 거쳐 접수처까지 진행되는 것을 확인했습니다. 세부 검사 범위는 [검증 기록](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.811/docs/RELEASE_STATUS.md)을 참고하세요.
- **구형 melonDS 코어의 게임 내부 저장·불러오기는 지원되지 않습니다.** 검사한 0.9.3 코어는 홈브루 SD를 제공하지 않아 “SD 카드를 읽을 수 없습니다”가 표시됩니다. 메뉴의 DSi SD 옵션은 별도 기능입니다.
- **melonDS DS**는 홈브루 SD를 지원합니다. PC용 melonDS 독립 실행판·Android APK는 별도 배포판이며, 이번 수정으로 해당 환경의 저장까지 검증한 것은 아닙니다.
- **R4 실기 재검증은 아직 없습니다.** 모든 R4 제품·커널에서 해결됐다는 의미는 아닙니다.

v0.81 패처로 만든 NDS는 이번 EXE에서 원본 CUE/BIN과 BIOS를 다시 지정하여 생성하세요. 기존 `snatcher_ds` 저장 폴더는 백업하고 보존합니다. 한글패치 기반은 계속 **Mega-CD 0.81**이며, v0.811은 DS 패처의 호환성 수정 버전입니다.

## 받기와 실행

**일반 사용자는 `Snatcher-DS-v0.811-Patcher.exe` 또는 `DS-Snatcher-Kr-MegaCD-Porting-v0.811-Patcher.zip`을 받으세요.** Windows 10/11 x64용이며 Python 설치가 필요 없습니다.

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

위 값은 **패처에 넣을 원본 파일**의 해시입니다. 패처에서는 연결된 **CUE**를 선택하세요. 여러 BIN 트랙으로 나뉜 원본은 각 파일의 해시가 위 단일 BIN 해시와 다르며 직접 비교할 수 없습니다. 첨부된 `INPUT-SHA256-v0.811.txt`에서도 확인할 수 있습니다.

## 게임 범위와 이전 검증

다음 ACT 1~3 주요 경로·엔딩 검사는 v0.81의 기록입니다. v0.811은 실행 코드·리소스를 그대로 유지하는 표식 수정이며 전체 경로 재완주 검사는 아닙니다.

- Mega-CD판 한글패치 0.81 자료 반영, ACT 1~3·엔딩 주요 경로 연결.
- BGM 계산 부하 완화, 상단 그림 비율·하단 초상화 배치 정리, 터치 조작과 5개 저장 슬롯, 클리어 후 숨겨진 ACT 선택.
- ROM·BIOS 파일을 고르는 Windows 단독 EXE 패처와 설명서·라이선스·대응 소스 제공.
- 실제 EXE로 1,230개 리소스와 NDS 생성 검증. 같은 NDS의 주요 경로 34구간 / 1,006,240 디스플레이 프레임, 엔딩과 재시작 후 클리어 기록 확인.

**버그·일부 구간/선택 분기의 진행 불가·특정 장면 사운드 끊김 가능성이 남아 있습니다.** PC 에뮬레이터의 주요 경로 검사이며 모든 실기·선택 분기를 검증한 것은 아닙니다. [알려진 문제](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.811/docs/KNOWN_ISSUES_KO.md) · [검증 기록](https://github.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/blob/v0.811/docs/RELEASE_STATUS.md)

## 실제 화면

| 접수처 | 보이스 대화 |
| --- | --- |
| ![접수처](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.811/screenshots/reception.png) | ![보이스 대화](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.811/screenshots/voice.png) |

| 터치 자판 | 플라토 거리 |
| --- | --- |
| ![터치 자판](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.811/screenshots/keyboard.png) | ![플라토 거리](https://raw.githubusercontent.com/OldManCraveGuitars-bit/DS-Snatcher-Kr-MegaCD-Porting/v0.811/screenshots/plato.png) |

화면은 v0.81 검증 NDS의 편집하지 않은 에뮬레이터 캡처입니다. v0.811에서는 화면과 리소스를 변경하지 않았습니다. 원본 게임·BIOS·완성 NDS는 배포하지 않습니다. 기존 v0.1Demo는 이전 배포로 보존합니다.
