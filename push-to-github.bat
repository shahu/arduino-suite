@echo off
chcp 65001 >nul
cd /d C:\Users\Administrator\Documents\Arduino
echo ============================================
echo  Pushing to GitHub (origin/main) ...
echo ============================================
git push origin main
echo.
if %errorlevel%==0 (
    echo [OK] Push succeeded!
) else (
    echo [FAIL] Push failed - see the output above.
    echo 常见原因: 凭证过期 / 无网络 / 远程有新提交(先 git pull)
)
echo.
pause
