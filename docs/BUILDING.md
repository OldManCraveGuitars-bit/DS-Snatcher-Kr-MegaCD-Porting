# v0.811 소스와 재빌드

일반 사용자는 EXE를 실행하면 됩니다. 한국어 기반은 Mega-CD 한글패치 0.81입니다.

`source-kit/`는 v0.81에서 사용한 v26 키트에서 `patcher/assemble_release.py`의 NitroFS 표식 누락만 수정한 대응 소스입니다. `package-files.json`에 수정한 파일의 해시를 반영했습니다. 플레이어 실행 코드와 변환 리소스는 변경하지 않았습니다. 키트 안에 남은 v26 봉인·검증 설명은 당시 기록이며 이번 차이는 이 문서를 따릅니다.

소스 패처는 `source-kit/README_KO.txt`와 `Patch-development.cmd`를 사용합니다. 원본 CUE/BIN과 지원 BIOS가 필요합니다. 플레이어·변환기 재빌드 자료와 개별 라이선스는 기존 키트에 보존했습니다.

## EXE 빌드

저장소 루트에 `source-kit/`와 `apps/windows_patcher/`를 둡니다. 배포 ZIP의 `source/` 폴더도 같은 구조입니다. Python 3.13, NumPy, Pillow, Unicorn, PyInstaller 6.22.3, pefile과 Tcl/Tk가 필요합니다.

동봉한 `pyinstaller-6.22.3.tar.gz`의 공식 소스를 풀고, Windows x64 MinGW 환경의 bootloader 폴더에서 `python waf all --gcc --no-cfg`로 런처를 빌드합니다. Unicorn longjmp 호환성을 위한 공식 소스 빌드이며 OS 보안 설정은 변경하지 않습니다.

```text
python -X utf8 -B apps/windows_patcher/test_nitrofs_marker.py
python -X utf8 -B apps/windows_patcher/build_exe.py --pyinstaller-source <풀어놓은-pyinstaller-6.22.3-폴더>
python -X utf8 -B apps/windows_patcher/verify_exe.py
```

검증 스크립트의 SOURCE/BIOS를 본인 원본 경로로 지정하세요. 빌드 산출물은 `work/exe-patcher-v0811/dist/Snatcher-DS-Patcher.exe`입니다. EXE의 내장 파일과 최종 NDS 해시를 검사합니다. 실제 마우스 클릭 검사는 별도이며 GUI 콜백 검사를 수동 검사로 표현하지 않습니다.

배포 포장은 `package_v0811.py`를 사용합니다. 이전 `package_exe.py`는 v0.81 내부 경로를 사용하는 역사적 스크립트입니다. 전체 EXE 변환 결과와 `docs/verification-v0.811.json`의 실행 검증이 있어야 새 포장을 만들 수 있습니다.

```text
python -X utf8 -B apps/windows_patcher/package_v0811.py --bootloader-source <pyinstaller-6.22.3.tar.gz>
```

소스에 원본 게임·BIOS·완성 NDS를 넣지 않습니다. EXE 빌드에는 원본이 필요하지 않으며, NDS 생성 단계에서 사용자가 제공한 원본을 읽습니다. 모든 기존 구성 요소의 라이선스와 소스 제공 조건은 유지합니다.
