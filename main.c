/*
gsimplemixer: a simple GTK mixer
Copyright (C) 2025 Fuzzy Dunlop

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <gtk/gtk.h>

#define SPACING 4

typedef struct {
	GtkWidget* container;
	GList* items;
} Mixer;

typedef struct {
	pa_context* context;
	uint32_t index;
	GtkWidget* button;
	GtkWidget* slider;
	GtkWidget* label;
	GtkWidget* controls;
} MixerItem;

static MixerItem* new_mixer_item(pa_context *c, uint32_t index, const char *icon_name, float volume, int muted) {
	MixerItem* mixer_item = g_new0(MixerItem, 1);
	mixer_item->context = c;
	mixer_item->index = index;

	mixer_item->button = gtk_toggle_button_new();
	gtk_button_set_icon_name(GTK_BUTTON(mixer_item->button), icon_name);
	gtk_widget_set_vexpand(mixer_item->button, FALSE);
	gtk_widget_set_valign(mixer_item->button, GTK_ALIGN_CENTER);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mixer_item->button), muted);

	mixer_item->slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 150.0, 0.5); /* page = 10 * step */
	gtk_widget_set_hexpand(mixer_item->slider, TRUE);
	gtk_range_set_value(GTK_RANGE(mixer_item->slider), volume);

	char volume_string[10];
	sprintf(volume_string, "%.0f%%", volume);
	mixer_item->label = gtk_label_new(volume_string);
	gtk_widget_set_size_request(mixer_item->label, 50, 0);
	gtk_label_set_xalign(GTK_LABEL(mixer_item->label), 1.0);
	PangoAttrList *attrlist = pango_attr_list_new();
	pango_attr_list_insert(attrlist, pango_attr_font_features_new("tnum=1"));
	gtk_label_set_attributes(GTK_LABEL(mixer_item->label), attrlist);

	mixer_item->controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
	gtk_box_set_spacing(GTK_BOX(mixer_item->controls), SPACING);
	gtk_box_append(GTK_BOX(mixer_item->controls), mixer_item->button);
	gtk_box_append(GTK_BOX(mixer_item->controls), mixer_item->slider);
	gtk_box_append(GTK_BOX(mixer_item->controls), mixer_item->label);

	return mixer_item;
}

static void remove_mixer_item(uint32_t index, void *userdata) {
	Mixer *mixer = userdata;
	GList *l = mixer->items;
	MixerItem *item;
	while (l != NULL) {
		item = l->data;
		if (item->index == index) {
			gtk_box_remove(GTK_BOX(mixer->container), item->controls);
			mixer->items = g_list_remove(mixer->items, item);
			g_free(item);
			break;
		}
		l = l->next;
	}
}

static pa_cvolume get_cvolume(GtkRange* range) {
	float value = gtk_range_get_value(GTK_RANGE(range)) / 100.0;
	pa_cvolume volume;
	volume.channels = 2;
	volume.values[0] = PA_VOLUME_NORM * value;
	volume.values[1] = PA_VOLUME_NORM * value;

	return volume;
}

static void set_volume_sink(GtkWidget *range, void *userdata) {
	MixerItem *sink = userdata;
	pa_cvolume volume = get_cvolume(GTK_RANGE(range));
	pa_context_set_sink_volume_by_index(sink->context, sink->index, &volume, NULL, NULL);
}

static void toggle_muted_sink(GtkWidget *toggle, void *userdata) {
	MixerItem *sink = userdata;
	pa_context_set_sink_mute_by_index(sink->context, sink->index, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)), NULL, NULL);
}

static void new_sink(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
	if (!eol) {
		const float volume = (float)pa_cvolume_avg(&(i->volume)) / PA_VOLUME_NORM * 100;

		Mixer *mixer = userdata;
		MixerItem* sink = new_mixer_item(c, i->index, "audio-volume-medium-symbolic", volume, i->mute);
		mixer->items = g_list_append(mixer->items, sink);

		g_signal_connect(sink->button, "clicked", G_CALLBACK(toggle_muted_sink), sink);
		g_signal_connect(sink->slider, "value-changed", G_CALLBACK(set_volume_sink), sink);

		gtk_box_prepend(GTK_BOX(mixer->container), sink->controls);
	}
}

static void change_sink(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
	if (!eol) {
		Mixer *mixer = userdata;
		GList *l = mixer->items;
		MixerItem *sink;
		while (l != NULL) {
			sink= l->data;
			if (sink->index == i->index) {
				const float volume = (float)pa_cvolume_avg(&(i->volume)) / PA_VOLUME_NORM * 100;
				char volume_string[10];
				sprintf(volume_string, "%.0f%%", volume);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sink->button), i->mute);
				gtk_range_set_value(GTK_RANGE(sink->slider), volume);
				gtk_label_set_text(GTK_LABEL(sink->label), volume_string);
				break;
			}
			l = l->next;
		}
	}
}

static void set_volume_sink_input(GtkWidget *range, void *userdata) {
	MixerItem *sink = userdata;
	float value = gtk_range_get_value(GTK_RANGE(range)) / 100.0;
	pa_cvolume volume = get_cvolume(GTK_RANGE(range));
	pa_context_set_sink_input_volume(sink->context, sink->index, &volume, NULL, NULL);
}

static void toggle_muted_sink_input(GtkWidget *toggle, void *userdata) {
	MixerItem *sink_input = userdata;
	pa_context_set_sink_input_mute(sink_input->context, sink_input->index, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)), NULL, NULL);
}

static void change_sink_input(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
	if (!eol) {
		Mixer *mixer = userdata;
		GList *l = mixer->items;
		MixerItem *sink_input;
		while (l != NULL) {
			sink_input = l->data;
			if (sink_input->index == i->index) {
				const float volume = (float)pa_cvolume_avg(&(i->volume)) / PA_VOLUME_NORM * 100;
				char volume_string[10];
				sprintf(volume_string, "%.0f%%", volume);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sink_input->button), i->mute);
				gtk_range_set_value(GTK_RANGE(sink_input->slider), volume);
				gtk_label_set_text(GTK_LABEL(sink_input->label), volume_string);
				break;
			}
			l = l->next;
		}
	}
}

static void new_sink_input(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
	if (!eol) {
		const char *icon_name = pa_proplist_gets(i->proplist, "application.icon_name");
		const char *binary_name = pa_proplist_gets(i->proplist, "application.process.binary");
		const float volume = (float)pa_cvolume_avg(&(i->volume)) / PA_VOLUME_NORM * 100;

		Mixer *mixer = userdata;
		MixerItem* sink_input = new_mixer_item(c, i->index, (icon_name == NULL) ? binary_name : icon_name, volume, i->mute);
		mixer->items = g_list_append(mixer->items, sink_input);

		g_signal_connect(sink_input->button, "clicked", G_CALLBACK(toggle_muted_sink_input), sink_input);
		g_signal_connect(sink_input->slider, "value-changed", G_CALLBACK(set_volume_sink_input), sink_input);

		gtk_box_append(GTK_BOX(mixer->container), sink_input->controls);
	}
}

static void context_subscribe_callback(pa_context *context, pa_subscription_event_type_t type, uint32_t index, void *data) {
	switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
			switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
				case PA_SUBSCRIPTION_EVENT_NEW:
					pa_context_get_sink_input_info(context, index, new_sink_input, data);
					break;
				case PA_SUBSCRIPTION_EVENT_CHANGE:
					pa_context_get_sink_input_info(context, index, change_sink_input, data);
					break;
				case PA_SUBSCRIPTION_EVENT_REMOVE:
					remove_mixer_item(index, data);
					break;
			}
			break;
		case PA_SUBSCRIPTION_EVENT_SINK:
			switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
				case PA_SUBSCRIPTION_EVENT_NEW:
					pa_context_get_sink_info_by_index(context, index, new_sink, data);
					break;
				case PA_SUBSCRIPTION_EVENT_CHANGE:
					pa_context_get_sink_info_by_index(context, index, change_sink, data);
					break;
				case PA_SUBSCRIPTION_EVENT_REMOVE:
					remove_mixer_item(index, data);
					break;
			}
			break;
	}
}

static void context_state_callback(pa_context *context, void *userdata){
	if (pa_context_get_state(context) == PA_CONTEXT_READY) {
		pa_context_get_sink_info_list(context, new_sink, userdata);
		pa_context_get_sink_input_info_list(context, new_sink_input, userdata);
		pa_context_set_subscribe_callback(context, context_subscribe_callback, userdata);
		pa_context_subscribe(context, PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);
	}
}

static void activate(GtkApplication *app, gpointer user_data) {
	Mixer* mixer = user_data;

	GtkWidget* window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Volume Control");
	gtk_window_set_default_size(GTK_WINDOW(window), 250, 0);
	gtk_window_set_resizable(GTK_WINDOW(window), false);

	mixer->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
	gtk_widget_set_margin_bottom(mixer->container, SPACING);
	gtk_widget_set_margin_top(mixer->container, SPACING);
	gtk_widget_set_margin_start(mixer->container, SPACING);
	gtk_widget_set_margin_end(mixer->container, SPACING);
	gtk_window_set_child(GTK_WINDOW(window), mixer->container);

	pa_glib_mainloop* mainloop = pa_glib_mainloop_new(g_main_context_default());
	pa_mainloop_api* api = pa_glib_mainloop_get_api(mainloop);
	pa_context* context = pa_context_new(api, NULL);
	pa_context_set_state_callback(context, context_state_callback, mixer);
	pa_context_connect(context, NULL, PA_CONTEXT_NOFAIL, NULL);

	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
	GtkApplication* app = gtk_application_new("org.gsimplemixer", G_APPLICATION_DEFAULT_FLAGS);
	Mixer* mixer = g_new0(Mixer, 1);
	g_signal_connect(app, "activate", G_CALLBACK(activate), mixer);
	int status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	g_free(mixer);

	return status;
}
