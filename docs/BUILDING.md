# v1.01 소스와 재빌드

일반 사용자는 Windows EXE를 실행합니다. 이 소스 키트는 Mega-CD 한글패치의 최종 검수 번역을 반영한 대응 소스입니다.
`source-kit/package-files.json`이 패처 내장 파일의 SHA-256을 고정합니다.

플레이어의 원본 의존 표·제어 그래프·미번역 원문은 배포에서 제거했으며, 사용자가 지정한 원본으로 복원합니다.
`source-kit/REBUILD-PLAYER_KO.txt`에 해당 툴체인, 원본/NDS에서 새 소스 트리 복원, ARM7/ARM9 빌드 및 고지 보존 절차가 있습니다.
새 실행 아이콘은 `player/assets/launcher-icon.gif`, 제작 프롬프트와 변환 기록은 `art/launcher/`에 있습니다.

## Windows EXE

Python 3.13 x64, NumPy, Pillow, Unicorn, PyInstaller 6.22.3, pefile, Tcl/Tk를 사용합니다.
동봉한 공식 PyInstaller 소스의 bootloader 폴더에서 Windows x64 MinGW로 `python waf all --gcc --no-cfg`를 실행합니다.
Unicorn longjmp와의 호환성을 위한 런처 빌드이며 OS 설정을 변경하지 않습니다.

```text
python -X utf8 -B apps/windows_patcher/build_exe.py --pyinstaller-source <PyInstaller 소스 폴더>
python -X utf8 -B apps/windows_patcher/verify_exe.py
```

검증 도구의 SOURCE/BIOS 경로는 본인 원본으로 지정합니다. 결과는 `work/exe-patcher-v101/dist/Snatcher-DS-Patcher.exe`입니다.
`verify_exe.py`는 GUI 콜백과 실제 동결 EXE 작업자를 검사합니다. 수동 데스크톱 클릭 검사와는 구분합니다.

배포 포장은 `package_v101.py --bootloader-source <공식-source.tar.gz>`를 사용합니다.
전체 EXE 변환 결과와 `docs/verification-v1.01.json`이 있어야 포장을 생성합니다.
이전 버전 포장 스크립트와 검증 파일은 역사 기록이며 현재 포장에는 사용하지 않습니다.
`source-kit/VERIFIED-BUILD.json`과 그 파일을 참조하는 재빌드 안내의 해시는 V0.81 전체 경로 검사 기록입니다.
현재 v1.01의 소스 재빌드·패처 출력 해시와 검증 범위는 `docs/verification-v1.01.json`을 기준으로 확인하세요.

원본 게임·BIOS·완성 NDS는 소스에 포함하지 않습니다. 기존 구성 요소의 라이선스와 소스 제공 조건을 유지합니다.
