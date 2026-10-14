# SPDX-License-Identifier: GPL-3.0-or-later
.class public L__BRIDGE__;
.super Landroid/app/Activity;

.field private static requested:Z
.field private launched:Z

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Landroid/app/Activity;-><init>()V
    return-void
.end method

.method protected onCreate(Landroid/os/Bundle;)V
    .locals 3
    invoke-super {p0, p1}, Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V
    # Every permission already granted: start the game without a dialog.
__PERMISSION_CHECKS__
    goto :continue
    :missing
    sget-boolean v1, L__BRIDGE__;->requested:Z
    if-eqz v1, :request
    # A same-process recreation waits for its pending permission callback.
    if-nez p1, :waiting
    goto :continue
    :request
    const/4 v1, 0x1
    sput-boolean v1, L__BRIDGE__;->requested:Z
    const-string v1, "OverportGXR"
    const-string v2, "Requesting XR permissions before game launch: __PERMISSION_NAMES__"
    invoke-static {v1, v2}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    const/16 v1, __PERMISSION_COUNT__
    new-array v1, v1, [Ljava/lang/String;
__PERMISSION_ARRAY__
    const/16 v2, 0x4758
    :try_start
    invoke-virtual {p0, v1, v2}, Landroid/app/Activity;->requestPermissions([Ljava/lang/String;I)V
    :try_end
    .catch Ljava/lang/RuntimeException; {:try_start .. :try_end} :request_failed
    :waiting
    return-void
    :request_failed
    move-exception v0
    const-string v1, "OverportGXR"
    const-string v2, "XR permission request unavailable; continuing game"
    invoke-static {v1, v2, v0}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I
    :continue
    invoke-direct {p0}, L__BRIDGE__;->launchGame()V
    return-void
.end method

.method public onRequestPermissionsResult(I[Ljava/lang/String;[I)V
    .locals 2
    invoke-super {p0, p1, p2, p3}, Landroid/app/Activity;->onRequestPermissionsResult(I[Ljava/lang/String;[I)V
    const/16 v0, 0x4758
    if-ne p1, v0, :done
    const-string v0, "OverportGXR"
    const-string v1, "XR permission response received; continuing game"
    invoke-static {v0, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    invoke-direct {p0}, L__BRIDGE__;->launchGame()V
    :done
    return-void
.end method

.method private launchGame()V
    .locals 3
    iget-boolean v0, p0, L__BRIDGE__;->launched:Z
    if-nez v0, :done
    const/4 v0, 0x1
    iput-boolean v0, p0, L__BRIDGE__;->launched:Z
    new-instance v0, Landroid/content/Intent;
    invoke-virtual {p0}, Landroid/app/Activity;->getIntent()Landroid/content/Intent;
    move-result-object v1
    invoke-direct {v0, v1}, Landroid/content/Intent;-><init>(Landroid/content/Intent;)V
    const-string v1, "__TARGET__"
    invoke-virtual {v0, p0, v1}, Landroid/content/Intent;->setClassName(Landroid/content/Context;Ljava/lang/String;)Landroid/content/Intent;
    # The helper task must not replace or clear an already running game task.
    const/4 v1, 0x0
    invoke-virtual {v0, v1}, Landroid/content/Intent;->setFlags(I)Landroid/content/Intent;
    const-string v1, "OverportGXR"
    const-string v2, "Permission gate complete; starting original activity"
    invoke-static {v1, v2}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    invoke-virtual {p0, v0}, Landroid/app/Activity;->startActivity(Landroid/content/Intent;)V
    invoke-virtual {p0}, Landroid/app/Activity;->finish()V
    :done
    return-void
.end method
